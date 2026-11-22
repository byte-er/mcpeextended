#include "RegionFile.h"
#include "../../../platform/log.h"
#include <zlib.h>
#include <vector>
#include <cassert>
#include <cstring>

// Chunk record layout inside chunks.dat:
//   legacy (0.6.1):  int32 length (incl. header) | raw chunk bytes
//   compressed:      int32 (length | COMPRESSED_FLAG) | int32 rawSize | zlib bytes
// Old uncompressed saves keep loading; everything written from now on is
// zlib compressed (~10x smaller, which matters with 512x512 worlds).
static const unsigned int COMPRESSED_FLAG = 0x80000000u;

const int SECTOR_BYTES = 4096;
const int SECTOR_INTS = SECTOR_BYTES / 4;
const int SECTOR_COLS = 32;

static const char* const REGION_DAT_NAME = "chunks.dat";

static void logAssert(int actual, int expected) {
	if (actual != expected) {
		LOGI("ERROR: I/O operation failed (%d vs %d)\n", actual, expected);
	}
}

RegionFile::RegionFile(const std::string& basePath, const char* fileName)
:	file(NULL)
{
	filename = basePath;
	filename += "/";
	filename += fileName ? fileName : REGION_DAT_NAME;

	offsets = new int[SECTOR_INTS];

	emptyChunk = new int[SECTOR_INTS];
	memset(emptyChunk, 0, SECTOR_INTS * sizeof(int));
}

RegionFile::~RegionFile()
{
	close();
	delete [] offsets;
	delete [] emptyChunk;
}

bool RegionFile::open()
{
	close();

	memset(offsets, 0, SECTOR_INTS * sizeof(int));

	// attempt to open file
	file = fopen(filename.c_str(), "r+b");
	if (file)
	{
		// read offset table
		logAssert(fread(offsets, sizeof(int), SECTOR_INTS, file), SECTOR_INTS);

		// mark initial sector as blocked
		sectorFree[0] = false;

		// setup blocked sectors
		for (int sector = 0; sector < SECTOR_INTS; sector++)
		{
			int offset = offsets[sector];
			if (offset)
			{
				int base = offset >> 8;
				int count = offset & 0xff;

				for (int i = 0; i < count; i++)
				{
					sectorFree[base + i] = false;
				}
			}
		}
	}
	else
	{
		// new world
		file = fopen(filename.c_str(), "w+b");
		if (!file)
		{
			LOGI("Failed to create chunk file %s\n", filename.c_str());
			return false;
		}

		// write sector header (all zeroes)
		logAssert(fwrite(offsets, sizeof(int), SECTOR_INTS, file), SECTOR_INTS);

		// mark initial sector as blocked
		sectorFree[0] = false;
	}

	return file != NULL;
}

void RegionFile::close()
{
	if (file)
	{
		fclose(file);
		file = NULL;
	}
}

bool RegionFile::readChunk(int x, int z, RakNet::BitStream** destChunkData)
{
	if (!file || x < 0 || z < 0 || x >= SECTOR_COLS || z >= SECTOR_COLS)
		return false;

	int offset = offsets[x + z * SECTOR_COLS];

	if (offset == 0)
	{
		// not written to file yet
		return false;
	}

	const int sectorNum = offset >> 8;
	const int maxBytes  = (offset & 0xff) * SECTOR_BYTES;

	if (fseek(file, sectorNum * SECTOR_BYTES, SEEK_SET) != 0)
		return false;

	unsigned int header = 0;
	if (fread(&header, sizeof(int), 1, file) != 1)
		return false;

	const bool compressed = (header & COMPRESSED_FLAG) != 0;
	int length = (int)(header & ~COMPRESSED_FLAG) - (int)sizeof(int);
	if (length <= 0 || length > maxBytes) {
		LOGI("ERROR: corrupt chunk record at %d, %d (len %d)\n", x, z, length);
		return false;
	}

	if (!compressed) {
		unsigned char* data = new unsigned char[length];
		if ((int)fread(data, 1, length, file) != length) {
			delete [] data;
			return false;
		}
		*destChunkData = new RakNet::BitStream(data, length, false);
		return true;
	}

	int rawSize = 0;
	if (fread(&rawSize, sizeof(int), 1, file) != 1 || rawSize <= 0 || rawSize > 4 * 1024 * 1024)
		return false;
	length -= sizeof(int);

	std::vector<unsigned char> packed(length);
	if ((int)fread(&packed[0], 1, length, file) != length)
		return false;

	unsigned char* data = new unsigned char[rawSize];
	uLongf destLen = (uLongf)rawSize;
	if (uncompress(data, &destLen, &packed[0], (uLong)length) != Z_OK || (int)destLen != rawSize) {
		LOGI("ERROR: failed to decompress chunk %d, %d\n", x, z);
		delete [] data;
		return false;
	}
	*destChunkData = new RakNet::BitStream(data, rawSize, false);
	return true;
}

bool RegionFile::writeChunk(int x, int z, RakNet::BitStream& rawData)
{
	if (!file || x < 0 || z < 0 || x >= SECTOR_COLS || z >= SECTOR_COLS)
		return false;

	// Compress the chunk; the record is [flagged length][raw size][zlib data]
	const uLong rawSize = rawData.GetNumberOfBytesUsed();
	uLongf packedSize = compressBound(rawSize);
	std::vector<unsigned char> packed(packedSize);
	if (compress2(&packed[0], &packedSize, rawData.GetData(), rawSize, Z_BEST_SPEED) != Z_OK) {
		LOGI("ERROR: failed to compress chunk %d, %d\n", x, z);
		return false;
	}
	RakNet::BitStream chunkData;
	const int rawSizeInt = (int)rawSize; // raw bytes: BitStream::Write(int) may byte-swap
	chunkData.Write((const char*)&rawSizeInt, (unsigned int)sizeof(int));
	chunkData.Write((const char*)&packed[0], (unsigned int)packedSize);

	int size = chunkData.GetNumberOfBytesUsed() + sizeof(int);

	int offset = offsets[x + z * SECTOR_COLS];
	int sectorNum = offset >> 8;
	int sectorCount = offset & 0xff;
	int sectorsNeeded = (size / SECTOR_BYTES) + 1;

	if (sectorsNeeded > 255) // sector count is stored in 8 bits
	{
		LOGI("ERROR: Chunk is too big to be saved to file\n");
		return false;
	}

	if (sectorNum != 0 && sectorCount == sectorsNeeded) {
		// the sector can be written on top of its old data
		write(sectorNum, chunkData);
	}
	else
	{
		// we need a new location

		// mark old sectors as free
		for (int i = 0; i < sectorCount; i++)
		{
			sectorFree[sectorNum + i] = true;
		}

		// find an available slot with enough run length
		int slot = 0;
		int runLength = 0;
		bool extendFile = false;
		while (runLength < sectorsNeeded)
		{
			if (sectorFree.find(slot + runLength) == sectorFree.end())
			{
				extendFile = true;
				break;
			}
			if (sectorFree[slot + runLength] == true)
			{
				runLength++;
			}
			else
			{
				slot = slot + runLength + 1;
				runLength = 0;
			}
		}

		if (extendFile)
		{
			fseek(file, 0, SEEK_END);
			for (int i = 0; i < (sectorsNeeded - runLength); i++)
			{
				fwrite(emptyChunk, sizeof(int), SECTOR_INTS, file);
				sectorFree[slot + i] = true;
			}
		}
		offsets[x + z * SECTOR_COLS] = (slot << 8) | sectorsNeeded;
		// mark slots as taken
		for (int i = 0; i < sectorsNeeded; i++)
		{
			sectorFree[slot + i] = false;
		}

		// write!
		write(slot, chunkData);

		// write sector data
		fseek(file, (x + z * SECTOR_COLS) * sizeof(int), SEEK_SET);
		fwrite(&offsets[x + z * SECTOR_COLS], sizeof(int), 1, file);
	}
	fflush(file);


	return true;
}

bool RegionFile::write(int sector, RakNet::BitStream& chunkData)
{
	fseek(file, sector * SECTOR_BYTES, SEEK_SET);
	//LOGI("writing %d B to sector %d\n", chunkData.GetNumberOfBytesUsed(), sector);
	unsigned int size = (unsigned int)(chunkData.GetNumberOfBytesUsed() + sizeof(int)) | COMPRESSED_FLAG;
	logAssert(fwrite(&size, sizeof(int), 1, file), 1);
	logAssert(fwrite(chunkData.GetData(), 1, chunkData.GetNumberOfBytesUsed(), file), chunkData.GetNumberOfBytesUsed());

	return true;
}
