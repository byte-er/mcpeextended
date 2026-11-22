#include "ImprovedNoise.h"
#include "../../../../util/Random.h"

ImprovedNoise::ImprovedNoise()
{
	Random random(1);
	init(&random);
}

ImprovedNoise::ImprovedNoise( Random* random )
{
	init(random);
}

void ImprovedNoise::init( Random* random )
{
	xo = random->nextFloat() * 256.f;
	yo = random->nextFloat() * 256.f;
	zo = random->nextFloat() * 256.f;
	for (int i = 0; i < 256; i++) {
		p[i] = i;
	}

	for (int i = 0; i < 256; i++) {
		int j = random->nextInt(256 - i) + i;
		int tmp = p[i];
		p[i] = p[j];
		p[j] = tmp;

		p[i + 256] = p[i];
	}
}

// Coordinates are doubles and floored to 64-bit: infinite worlds sample the
// noise millions of blocks out, where floats (and 32-bit ints) fall apart.
static inline long long lfloor(double v) { long long i = (long long)v; return v < (double)i ? i - 1 : i; }

float ImprovedNoise::noise( double _x, double _y, double _z )
{
	const double dx = _x + xo, dy = _y + yo, dz = _z + zo;
	const long long xf = lfloor(dx), yf = lfloor(dy), zf = lfloor(dz);

	int X = (int)(xf & 255), // FIND UNIT CUBE THAT
		Y = (int)(yf & 255), // CONTAINS POINT.
		Z = (int)(zf & 255);

	float x = (float)(dx - (double)xf); // FIND RELATIVE X,Y,Z
	float y = (float)(dy - (double)yf); // OF POINT IN CUBE.
	float z = (float)(dz - (double)zf);

	float u = x * x * x * (x * (x * 6 - 15) + 10), // COMPUTE FADE CURVES
		v = y * y * y * (y * (y * 6 - 15) + 10), // FOR EACH OF X,Y,Z.
		w = z * z * z * (z * (z * 6 - 15) + 10);

	int A = p[X] + Y, AA = p[A] + Z, AB = p[A + 1] + Z, // HASH COORDINATES OF
		B = p[X + 1] + Y, BA = p[B] + Z, BB = p[B + 1] + Z; // THE 8 CUBE CORNERS,

	return lerp(w, lerp(v, lerp(u, grad(p[AA], x, y, z), // AND ADD
		grad(p[BA], x - 1, y, z)), // BLENDED
		lerp(u, grad(p[AB], x, y - 1, z), // RESULTS
		grad(p[BB], x - 1, y - 1, z))),// FROM  8
		lerp(v, lerp(u, grad(p[AA + 1], x, y, z - 1), // CORNERS
		grad(p[BA + 1], x - 1, y, z - 1)), // OF CUBE
		lerp(u, grad(p[AB + 1], x, y - 1, z - 1), grad(p[BB + 1], x - 1, y - 1, z - 1))));
}

const float ImprovedNoise::lerp( float t, float a, float b )
{
	return a + t * (b - a);
}

const float ImprovedNoise::grad2( int hash, float x, float z )
{
	int h = hash & 15; // CONVERT LO 4 BITS OF HASH CODE

	float u = (1-((h&8)>>3))*x, // INTO 12 GRADIENT DIRECTIONS.
		v = h < 4 ? 0 : h == 12 || h == 14 ? x : z;

	return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

const float ImprovedNoise::grad( int hash, float x, float y, float z )
{
	int h = hash & 15; // CONVERT LO 4 BITS OF HASH CODE

	float u = h < 8 ? x : y, // INTO 12 GRADIENT DIRECTIONS.
		v = h < 4 ? y : h == 12 || h == 14 ? x : z;

	return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

float ImprovedNoise::getValue( float x, float y )
{
	return noise(x, y, 0);
}

float ImprovedNoise::getValue( float x, float y, float z )
{
	return noise(x, y, z);
}

void ImprovedNoise::add( float* buffer, double _x, double _y, double _z, int xSize, int ySize, int zSize, double xs, double ys, double zs, float pow )
{
	if (ySize==1) {
		int A = 0, AA = 0, B = 0, BA = 0;
		float vv0 = 0, vv2 = 0;
		int pp = 0;
		float scale = 1.0f / pow;
		for (int xx = 0; xx < xSize; xx++) {
			const double dx = (_x + xx) * xs + xo;
			const long long xf = lfloor(dx);
			int X = (int)(xf & 255);
			float x = (float)(dx - (double)xf);
			float u = x * x * x * (x * (x * 6 - 15) + 10);

			for (int zz = 0; zz < zSize; zz++) {
				const double dz = (_z + zz) * zs + zo;
				const long long zf = lfloor(dz);
				int Z = (int)(zf & 255);
				float z = (float)(dz - (double)zf);
				float w = z * z * z * (z * (z * 6 - 15) + 10);

				A = p[X] + 0;
				AA = p[A] + Z;
				B = p[X + 1] + 0;
				BA = p[B] + Z;
				vv0 = lerp(u, grad2(p[AA], x, z), grad(p[BA], x - 1, 0, z));
				vv2 = lerp(u, grad(p[AA + 1], x, 0, z - 1), grad(p[BA + 1], x - 1, 0, z - 1));

				float val = lerp(w, vv0, vv2);

				buffer[pp++] += val * scale;
			}
		}
		return;
	}
	int pp = 0;
	float scale = 1 / pow;
	int yOld = -1;
	int A = 0, AA = 0, AB = 0, B = 0, BA = 0, BB = 0;
	float vv0 = 0, vv1 = 0, vv2 = 0, vv3 = 0;

	for (int xx = 0; xx < xSize; xx++) {
		const double dx = (_x + xx) * xs + xo;
		const long long xf = lfloor(dx);
		int X = (int)(xf & 255);
		float x = (float)(dx - (double)xf);
		float u = x * x * x * (x * (x * 6 - 15) + 10);


		for (int zz = 0; zz < zSize; zz++) {
			const double dz = (_z + zz) * zs + zo;
			const long long zf = lfloor(dz);
			int Z = (int)(zf & 255);
			float z = (float)(dz - (double)zf);
			float w = z * z * z * (z * (z * 6 - 15) + 10);

			for (int yy = 0; yy < ySize; yy++) {
				const double dy = (_y + yy) * ys + yo;
				const long long yf = lfloor(dy);
				int Y = (int)(yf & 255);
				float y = (float)(dy - (double)yf);
				float v = y * y * y * (y * (y * 6 - 15) + 10);

				if (yy == 0 || Y != yOld) {
					yOld = Y;
					A = p[X] + Y;
					AA = p[A] + Z;
					AB = p[A + 1] + Z;
					B = p[X + 1] + Y;
					BA = p[B] + Z;
					BB = p[B + 1] + Z;
					vv0 = lerp(u, grad(p[AA], x, y, z), grad(p[BA], x - 1, y, z));
					vv1 = lerp(u, grad(p[AB], x, y - 1, z), grad(p[BB], x - 1, y - 1, z));
					vv2 = lerp(u, grad(p[AA + 1], x, y, z - 1), grad(p[BA + 1], x - 1, y, z - 1));
					vv3 = lerp(u, grad(p[AB + 1], x, y - 1, z - 1), grad(p[BB + 1], x - 1, y - 1, z - 1));
				}

				float v0 = lerp(v, vv0, vv1);
				float v1 = lerp(v, vv2, vv3);
				float val = lerp(w, v0, v1);

				buffer[pp++] += val * scale;
			}
		}
	}
}

int ImprovedNoise::hashCode() {
    int x = 4711;
    for (int i = 0; i < 512; ++i)
        x = x * 37 + p[i];
    return x;
}

