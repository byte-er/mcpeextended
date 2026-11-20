#ifndef NET_MINECRAFT_NETWORK_EAGLER__EaglerPacket_H__
#define NET_MINECRAFT_NETWORK_EAGLER__EaglerPacket_H__

#include <string>
#include <vector>
#include <cstring>
#include <stdint.h>

/**
 * Minecraft 1.8 (protocol 47) wire format helpers: big-endian numbers,
 * VarInts, UTF-8 strings and packed block positions. Reads past the end set
 * `err` and return zeros instead of crashing on malformed packets.
 */
class PacketIn
{
public:
	PacketIn(const unsigned char* data, size_t len) : p(data), n(len), i(0), err(false) {}

	bool ok() const { return !err; }
	size_t remaining() const { return i <= n ? n - i : 0; }
	const unsigned char* cur() const { return p + i; }

	uint8_t u8() { if (i + 1 > n) { err = true; return 0; } return p[i++]; }
	int8_t  i8() { return (int8_t)u8(); }
	bool    boolean() { return u8() != 0; }
	uint16_t u16() { if (i + 2 > n) { err = true; i = n; return 0; } uint16_t v = (uint16_t)((p[i] << 8) | p[i + 1]); i += 2; return v; }
	int16_t  i16() { return (int16_t)u16(); }
	int32_t  i32() { if (i + 4 > n) { err = true; i = n; return 0; }
		uint32_t v = ((uint32_t)p[i] << 24) | ((uint32_t)p[i + 1] << 16) | ((uint32_t)p[i + 2] << 8) | p[i + 3]; i += 4; return (int32_t)v; }
	int64_t  i64() { uint64_t hi = (uint32_t)i32(); uint64_t lo = (uint32_t)i32(); return (int64_t)((hi << 32) | lo); }
	float    f32() { int32_t v = i32(); float f; memcpy(&f, &v, 4); return f; }
	double   f64() { int64_t v = i64(); double d; memcpy(&d, &v, 8); return d; }

	int32_t varint() {
		uint32_t v = 0; int shift = 0;
		for (int k = 0; k < 5; ++k) {
			uint8_t b = u8();
			v |= (uint32_t)(b & 0x7F) << shift;
			if (!(b & 0x80)) return (int32_t)v;
			shift += 7;
		}
		err = true; return 0;
	}
	int64_t varlong() {
		uint64_t v = 0; int shift = 0;
		for (int k = 0; k < 10; ++k) {
			uint8_t b = u8();
			v |= (uint64_t)(b & 0x7F) << shift;
			if (!(b & 0x80)) return (int64_t)v;
			shift += 7;
		}
		err = true; return 0;
	}
	std::string str() {
		int len = varint();
		if (len < 0 || i + (size_t)len > n) { err = true; i = n; return std::string(); }
		std::string s((const char*)p + i, len); i += len; return s;
	}
	void position(int& x, int& y, int& z) {
		int64_t v = i64();
		x = (int)(v >> 38);
		y = (int)((v >> 26) & 0xFFF);
		z = (int)((v << 38) >> 38);
		if (y >= 2048) y -= 4096;
	}
	void skip(size_t k) { if (i + k > n) { err = true; i = n; } else i += k; }
	void bytes(void* out, size_t k) { if (i + k > n) { err = true; i = n; memset(out, 0, k); return; } memcpy(out, p + i, k); i += k; }

private:
	const unsigned char* p;
	size_t n, i;
	bool err;
};

class PacketOut
{
public:
	explicit PacketOut(int packetId) { varint(packetId); }
	PacketOut() {}

	std::vector<unsigned char> buf;

	void u8(int v)  { buf.push_back((unsigned char)v); }
	void boolean(bool b) { u8(b ? 1 : 0); }
	void u16(int v) { u8(v >> 8); u8(v); }
	void i32(int32_t v) { u8(v >> 24); u8(v >> 16); u8(v >> 8); u8(v); }
	void i64(int64_t v) { i32((int32_t)(v >> 32)); i32((int32_t)v); }
	void f32(float f) { int32_t v; memcpy(&v, &f, 4); i32(v); }
	void f64(double d) { int64_t v; memcpy(&v, &d, 8); i64(v); }
	void varint(int32_t value) {
		uint32_t v = (uint32_t)value;
		do { unsigned char b = v & 0x7F; v >>= 7; if (v) b |= 0x80; buf.push_back(b); } while (v);
	}
	void str(const std::string& s) { varint((int)s.size()); buf.insert(buf.end(), s.begin(), s.end()); }
	void str8(const std::string& s) { u8((int)s.size()); buf.insert(buf.end(), s.begin(), s.end()); } // handshake strings
	void position(int x, int y, int z) {
		int64_t v = ((int64_t)(x & 0x3FFFFFF) << 38) | ((int64_t)(y & 0xFFF) << 26) | (int64_t)(z & 0x3FFFFFF);
		i64(v);
	}
	void raw(const void* d, size_t k) { const unsigned char* c = (const unsigned char*)d; buf.insert(buf.end(), c, c + k); }
};

#endif /*NET_MINECRAFT_NETWORK_EAGLER__EaglerPacket_H__*/
