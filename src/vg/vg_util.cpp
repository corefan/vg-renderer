#include "vg_util.h"
#include "vg.h"
#include <bx/bx.h>

#if VG_CONFIG_ENABLE_SIMD && BX_CPU_X86
#include <xmmintrin.h>
#include <immintrin.h>
#endif

namespace vgutil
{
bool invertMatrix3(const float* __restrict t, float* __restrict inv)
{
	// nvgTransformInverse
	double invdet, det = (double)t[0] * t[3] - (double)t[2] * t[1];
	if (det > -1e-6 && det < 1e-6) {
		inv[0] = inv[2] = 1.0f;
		inv[1] = inv[3] = inv[4] = inv[5] = 0.0f;
		return false;
	}

	invdet = 1.0 / det;
	inv[0] = (float)(t[3] * invdet);
	inv[2] = (float)(-t[2] * invdet);
	inv[4] = (float)(((double)t[2] * t[5] - (double)t[3] * t[4]) * invdet);
	inv[1] = (float)(-t[1] * invdet);
	inv[3] = (float)(t[0] * invdet);
	inv[5] = (float)(((double)t[1] * t[4] - (double)t[0] * t[5]) * invdet);

	return true;
}

void memset32(void* __restrict dst, uint32_t n, const void* __restrict src)
{
#if VG_CONFIG_ENABLE_SIMD && BX_CPU_X86
	const __m128 s128 = _mm_load_ps1((const float*)src);
	float* d = (float*)dst;

	uint32_t iter = n >> 4;
	while (iter-- > 0) {
		_mm_storeu_ps(d + 0, s128);
		_mm_storeu_ps(d + 4, s128);
		_mm_storeu_ps(d + 8, s128);
		_mm_storeu_ps(d + 12, s128);
		d += 16;
	}

	uint32_t rem = n & 15;
	if (rem >= 8) {
		_mm_storeu_ps(d + 0, s128);
		_mm_storeu_ps(d + 4, s128);
		d += 8;
		rem -= 8;
	}

	if (rem >= 4) {
		_mm_storeu_ps(d, s128);
		d += 4;
		rem -= 4;
	}

	switch (rem) {
	case 3: *d++ = *(const float*)src;
	case 2: *d++ = *(const float*)src;
	case 1: *d   = *(const float*)src;
	}
#else
	const uint32_t s = *(const uint32_t*)src;
	uint32_t* d = (uint32_t*)dst;
	while (n-- > 0) {
		*d++ = s;
	}
#endif
}

void memset64(void* __restrict dst, uint32_t n64, const void* __restrict src)
{
#if VG_CONFIG_ENABLE_SIMD && BX_CPU_X86
	const float* s = (const float*)src;
	const __m128 s0 = _mm_load_ss(&s[0]);
	const __m128 s1 = _mm_load_ss(&s[1]);
	const __m128 s0011 = _mm_shuffle_ps(s0, s1, 0);
	const __m128 s128 = _mm_shuffle_ps(s0011, s0011, _MM_SHUFFLE(2, 0, 2, 0));
	float* d = (float*)dst;

	uint32_t iter = n64 >> 3; // 8 64-bit values per iteration (== 16 floats / iter)
	while (iter-- > 0) {
		_mm_storeu_ps(d + 0, s128);
		_mm_storeu_ps(d + 4, s128);
		_mm_storeu_ps(d + 8, s128);
		_mm_storeu_ps(d + 12, s128);
		d += 16;
	}

	uint32_t rem = n64 & 7;
	if (rem >= 4) {
		_mm_storeu_ps(d + 0, s128);
		_mm_storeu_ps(d + 4, s128);
		d += 8;
		rem -= 4;
	}

	if (rem >= 2) {
		_mm_storeu_ps(d, s128);
		d += 4;
		rem -= 2;
	}

	if (rem) {
		d[0] = s[0];
		d[1] = s[1];
	}
#else
	const uint32_t s0 = *((const uint32_t*)src + 0);
	const uint32_t s1 = *((const uint32_t*)src + 1);
	uint32_t* d = (uint32_t*)dst;
	while (n64-- > 0) {
		d[0] = s0;
		d[1] = s1;
		d += 2;
	}
#endif
}

void memset128(void* __restrict dst, uint32_t n128, const void* __restrict src)
{
#if VG_CONFIG_ENABLE_SIMD && BX_CPU_X86
	const __m128 s128 = _mm_loadu_ps((const float*)src);
	float* d = (float*)dst;

	uint32_t iter = n128 >> 2; // 4 128-bit values per iteration (== 16 floats / iter)
	while (iter-- > 0) {
		_mm_storeu_ps(d + 0, s128);
		_mm_storeu_ps(d + 4, s128);
		_mm_storeu_ps(d + 8, s128);
		_mm_storeu_ps(d + 12, s128);
		d += 16;
	}

	uint32_t rem = n128 & 3;
	if (rem >= 2) {
		_mm_storeu_ps(d + 0, s128);
		_mm_storeu_ps(d + 4, s128);
		d += 8;
		rem -= 2;
	}

	if (rem) {
		_mm_storeu_ps(d, s128);
	}
#else
	const uint32_t s0 = *((const uint32_t*)src + 0);
	const uint32_t s1 = *((const uint32_t*)src + 1);
	const uint32_t s2 = *((const uint32_t*)src + 2);
	const uint32_t s3 = *((const uint32_t*)src + 3);
	uint32_t* d = (uint32_t*)dst;
	while (n128-- > 0) {
		d[0] = s0;
		d[1] = s1;
		d[2] = s2;
		d[3] = s3;
		d += 4;
	}
#endif
}

void genQuadIndices_unaligned(uint16_t* dst, uint32_t n, uint16_t firstVertexID)
{
#if VG_CONFIG_ENABLE_SIMD && BX_CPU_X86
	BX_ALIGN_DECL(16, static const uint16_t delta[]) = {
		0, 1, 2, 0, 2, 3,
		4, 5, 6, 4, 6, 7,
		8, 9, 10, 8, 10, 11,
		12, 13, 14, 12, 14, 15
	};

	const __m128i xmm_delta0 = _mm_load_si128((const __m128i*)&delta[0]);
	const __m128i xmm_delta1 = _mm_load_si128((const __m128i*)&delta[8]);
	const __m128i xmm_delta2 = _mm_load_si128((const __m128i*)&delta[16]);

	const uint32_t numIter = n >> 2; // 4 quads per iteration
	for (uint32_t i = 0; i < numIter; ++i) {
		const __m128i id = _mm_set1_epi16(firstVertexID);

		const __m128i id0 = _mm_add_epi16(id, xmm_delta0);
		const __m128i id1 = _mm_add_epi16(id, xmm_delta1);
		const __m128i id2 = _mm_add_epi16(id, xmm_delta2);
		_mm_storeu_si128((__m128i*)(dst + 0), id0);
		_mm_storeu_si128((__m128i*)(dst + 8), id1);
		_mm_storeu_si128((__m128i*)(dst + 16), id2);

		dst += 24;
		firstVertexID += 16;
	}

	uint32_t rem = n & 3;
	switch (rem) {
	case 3:
		dst[0] = firstVertexID; dst[1] = firstVertexID + 1; dst[2] = firstVertexID + 2;
		dst[3] = firstVertexID; dst[4] = firstVertexID + 2; dst[5] = firstVertexID + 3;
		dst += 6;
		firstVertexID += 4;
	case 2:
		dst[0] = firstVertexID; dst[1] = firstVertexID + 1; dst[2] = firstVertexID + 2;
		dst[3] = firstVertexID; dst[4] = firstVertexID + 2; dst[5] = firstVertexID + 3;
		dst += 6;
		firstVertexID += 4;
	case 1:
		dst[0] = firstVertexID; dst[1] = firstVertexID + 1; dst[2] = firstVertexID + 2;
		dst[3] = firstVertexID; dst[4] = firstVertexID + 2; dst[5] = firstVertexID + 3;
		dst += 6;
		firstVertexID += 4;
	}
#else
	while (n-- > 0) {
		dst[0] = firstVertexID; dst[1] = firstVertexID + 1; dst[2] = firstVertexID + 2;
		dst[3] = firstVertexID; dst[4] = firstVertexID + 2; dst[5] = firstVertexID + 3;
		dst += 6;
		firstVertexID += 4;
	}
#endif
}

void batchTransformTextQuads(const float* __restrict quads, uint32_t n, const float* __restrict mtx, float* __restrict transformedVertices)
{
#if VG_CONFIG_ENABLE_SIMD
	const bx::simd128_t mtx0 = bx::simd_splat(mtx[0]);
	const bx::simd128_t mtx1 = bx::simd_splat(mtx[1]);
	const bx::simd128_t mtx2 = bx::simd_splat(mtx[2]);
	const bx::simd128_t mtx3 = bx::simd_splat(mtx[3]);
	const bx::simd128_t mtx4 = bx::simd_splat(mtx[4]);
	const bx::simd128_t mtx5 = bx::simd_splat(mtx[5]);

	const uint32_t iter = n >> 1; // 2 quads per iteration
	for (uint32_t i = 0; i < iter; ++i) {
		bx::simd128_t q0 = bx::simd_ld(quads);     // (x0, y0, x1, y1)
		bx::simd128_t q1 = bx::simd_ld(quads + 8); // (x2, y2, x3, y3)

		bx::simd128_t x0123 = bx::simd_shuf_xyAB(bx::simd_swiz_xzxz(q0), bx::simd_swiz_xzxz(q1)); // (x0, x1, x2, x3)
		bx::simd128_t y0123 = bx::simd_shuf_xyAB(bx::simd_swiz_ywyw(q0), bx::simd_swiz_ywyw(q1)); // (y0, y1, y2, y3)
		bx::simd128_t x0123_m0 = bx::simd_mul(x0123, mtx0); // (x0, x1, x2, x3) * mtx[0]
		bx::simd128_t x0123_m1 = bx::simd_mul(x0123, mtx1); // (x0, x1, x2, x3) * mtx[1]
		bx::simd128_t y0123_m2 = bx::simd_mul(y0123, mtx2); // (y0, y1, y2, y3) * mtx[2]
		bx::simd128_t y0123_m3 = bx::simd_mul(y0123, mtx3); // (y0, y1, y2, y3) * mtx[3]

		// v0.x = x0_m0 + y0_m2 + m4
		// v1.x = x1_m0 + y0_m2 + m4
		// v2.x = x1_m0 + y1_m2 + m4
		// v3.x = x0_m0 + y1_m2 + m4
		// v0.y = x0_m1 + y0_m3 + m5
		// v1.y = x1_m1 + y0_m3 + m5
		// v2.y = x1_m1 + y1_m3 + m5
		// v3.y = x0_m1 + y1_m3 + m5
		bx::simd128_t x0110_m0 = bx::simd_swiz_xyyx(x0123_m0);
		bx::simd128_t x0110_m1 = bx::simd_swiz_xyyx(x0123_m1);
		bx::simd128_t y0011_m2 = bx::simd_swiz_xxyy(y0123_m2);
		bx::simd128_t y0011_m3 = bx::simd_swiz_xxyy(y0123_m3);

		bx::simd128_t v0123_x = bx::simd_add(x0110_m0, bx::simd_add(y0011_m2, mtx4));
		bx::simd128_t v0123_y = bx::simd_add(x0110_m1, bx::simd_add(y0011_m3, mtx5));

		bx::simd128_t v01 = bx::simd_swiz_xzyw(bx::simd_shuf_xyAB(v0123_x, v0123_y));
		bx::simd128_t v23 = bx::simd_swiz_xzyw(bx::simd_shuf_zwCD(v0123_x, v0123_y));

		bx::simd_st(transformedVertices, v01);
		bx::simd_st(transformedVertices + 4, v23);

		// v4.x = x2_m0 + y2_m2 + m4
		// v5.x = x3_m0 + y2_m2 + m4
		// v6.x = x3_m0 + y3_m2 + m4
		// v7.x = x2_m0 + y3_m2 + m4
		// v4.y = x2_m1 + y2_m3 + m5
		// v5.y = x3_m1 + y2_m3 + m5
		// v6.y = x3_m1 + y3_m3 + m5
		// v7.y = x2_m1 + y3_m3 + m5
		bx::simd128_t x2332_m0 = bx::simd_swiz_zwwz(x0123_m0);
		bx::simd128_t x2332_m1 = bx::simd_swiz_zwwz(x0123_m1);
		bx::simd128_t y2233_m2 = bx::simd_swiz_zzww(y0123_m2);
		bx::simd128_t y2233_m3 = bx::simd_swiz_zzww(y0123_m3);

		bx::simd128_t v4567_x = bx::simd_add(x2332_m0, bx::simd_add(y2233_m2, mtx4));
		bx::simd128_t v4567_y = bx::simd_add(x2332_m1, bx::simd_add(y2233_m3, mtx5));

		bx::simd128_t v45 = bx::simd_swiz_xzyw(bx::simd_shuf_xyAB(v4567_x, v4567_y));
		bx::simd128_t v67 = bx::simd_swiz_xzyw(bx::simd_shuf_zwCD(v4567_x, v4567_y));

		bx::simd_st(transformedVertices + 8, v45);
		bx::simd_st(transformedVertices + 12, v67);

		quads += 16;
		transformedVertices += 16;
	}

	const uint32_t rem = n & 1;
	if (rem) {
		bx::simd128_t q0 = bx::simd_ld(quads);

		bx::simd128_t x0101 = bx::simd_swiz_xzxz(q0); // (x0, x1, x0, x1)
		bx::simd128_t y0101 = bx::simd_swiz_ywyw(q0); // (y0, y1, y0, y1)
		bx::simd128_t x0101_m0 = bx::simd_mul(x0101, mtx0); // (x0, x1, x0, x1) * mtx[0]
		bx::simd128_t x0101_m1 = bx::simd_mul(x0101, mtx1); // (x0, x1, x0, x1) * mtx[1]
		bx::simd128_t y0101_m2 = bx::simd_mul(y0101, mtx2); // (y0, y1, y0, y1) * mtx[2]
		bx::simd128_t y0101_m3 = bx::simd_mul(y0101, mtx3); // (y0, y1, y0, y1) * mtx[3]

		// v0.x = x0_m0 + y0_m2 + m4
		// v1.x = x1_m0 + y0_m2 + m4
		// v2.x = x1_m0 + y1_m2 + m4
		// v3.x = x0_m0 + y1_m2 + m4
		// v0.y = x0_m1 + y0_m3 + m5
		// v1.y = x1_m1 + y0_m3 + m5
		// v2.y = x1_m1 + y1_m3 + m5
		// v3.y = x0_m1 + y1_m3 + m5
		bx::simd128_t x0110_m0 = bx::simd_swiz_xyyx(x0101_m0);
		bx::simd128_t x0110_m1 = bx::simd_swiz_xyyx(x0101_m1);
		bx::simd128_t y0011_m2 = bx::simd_swiz_xxyy(y0101_m2);
		bx::simd128_t y0011_m3 = bx::simd_swiz_xxyy(y0101_m3);

		bx::simd128_t v0123_x = bx::simd_add(x0110_m0, bx::simd_add(y0011_m2, mtx4));
		bx::simd128_t v0123_y = bx::simd_add(x0110_m1, bx::simd_add(y0011_m3, mtx5));

		bx::simd128_t v01 = bx::simd_swiz_xzyw(bx::simd_shuf_xyAB(v0123_x, v0123_y));
		bx::simd128_t v23 = bx::simd_swiz_xzyw(bx::simd_shuf_zwCD(v0123_x, v0123_y));

		bx::simd_st(transformedVertices, v01);
		bx::simd_st(transformedVertices + 4, v23);
	}
#else
	for (uint32_t i = 0; i < n; ++i) {
		const float* q = &quads[i * 8];
		const uint32_t s = i << 3;
		transformPos2D(q[0], q[1], mtx, &transformedVertices[s + 0]);
		transformPos2D(q[2], q[1], mtx, &transformedVertices[s + 2]);
		transformPos2D(q[2], q[3], mtx, &transformedVertices[s + 4]);
		transformPos2D(q[0], q[3], mtx, &transformedVertices[s + 6]);
	}
#endif
}

void batchTransformPositions(const float* __restrict v, uint32_t n, float* __restrict p, const float* __restrict mtx)
{
#if VG_CONFIG_ENABLE_SIMD
	const float* src = v;
	float* dst = p;

	const bx::simd128_t mtx0 = bx::simd_splat(mtx[0]);
	const bx::simd128_t mtx1 = bx::simd_splat(mtx[1]);
	const bx::simd128_t mtx2 = bx::simd_splat(mtx[2]);
	const bx::simd128_t mtx3 = bx::simd_splat(mtx[3]);
	const bx::simd128_t mtx4 = bx::simd_splat(mtx[4]);
	const bx::simd128_t mtx5 = bx::simd_splat(mtx[5]);

	const uint32_t iter = n >> 2;
	for (uint32_t i = 0; i < iter; ++i) {
		bx::simd128_t src0123 = bx::simd_ld(src);
		bx::simd128_t src4567 = bx::simd_ld(src + 4);

		bx::simd128_t src0246 = bx::simd_shuf_xyAB(bx::simd_swiz_xzxz(src0123), bx::simd_swiz_xzxz(src4567));
		bx::simd128_t src1357 = bx::simd_shuf_xyAB(bx::simd_swiz_ywyw(src0123), bx::simd_swiz_ywyw(src4567));

		bx::simd128_t dst0246 = bx::simd_add(bx::simd_add(bx::simd_mul(src0246, mtx0), bx::simd_mul(src1357, mtx2)), mtx4);
		bx::simd128_t dst1357 = bx::simd_add(bx::simd_add(bx::simd_mul(src0246, mtx1), bx::simd_mul(src1357, mtx3)), mtx5);

		bx::simd128_t dst0123 = bx::simd_swiz_xzyw(bx::simd_shuf_xyAB(dst0246, dst1357));
		bx::simd128_t dst4567 = bx::simd_swiz_xzyw(bx::simd_shuf_zwCD(dst0246, dst1357));

		bx::simd_st(dst, dst0123);
		bx::simd_st(dst + 4, dst4567);

		src += 8;
		dst += 8;
	}

	const uint32_t rem = n & 3;
	switch (rem) {
	case 3:
		*dst++ = mtx[0] * src[0] + mtx[2] * src[1] + mtx[4];
		*dst++ = mtx[1] * src[0] + mtx[3] * src[1] + mtx[5];
		src += 2;
	case 2:
		*dst++ = mtx[0] * src[0] + mtx[2] * src[1] + mtx[4];
		*dst++ = mtx[1] * src[0] + mtx[3] * src[1] + mtx[5];
		src += 2;
	case 1:
		*dst++ = mtx[0] * src[0] + mtx[2] * src[1] + mtx[4];
		*dst++ = mtx[1] * src[0] + mtx[3] * src[1] + mtx[5];
		src += 2;
	}
#else
	for (uint32_t i = 0; i < n; ++i) {
		const uint32_t id = i << 1;
		transformPos2D(v[id], v[id + 1], mtx, &p[id]);
	}
#endif
}

void batchTransformPositions_Unaligned(const float* __restrict v, uint32_t n, float* __restrict p, const float* __restrict mtx)
{
#if VG_CONFIG_ENABLE_SIMD && BX_CPU_X86
	const float* src = v;
	float* dst = p;

	const bx::simd128_t mtx0 = bx::simd_splat(mtx[0]);
	const bx::simd128_t mtx1 = bx::simd_splat(mtx[1]);
	const bx::simd128_t mtx2 = bx::simd_splat(mtx[2]);
	const bx::simd128_t mtx3 = bx::simd_splat(mtx[3]);
	const bx::simd128_t mtx4 = bx::simd_splat(mtx[4]);
	const bx::simd128_t mtx5 = bx::simd_splat(mtx[5]);

	const uint32_t iter = n >> 2;
	for (uint32_t i = 0; i < iter; ++i) {
		bx::simd128_t src0123 = _mm_loadu_ps(src);
		bx::simd128_t src4567 = _mm_loadu_ps(src + 4);

		bx::simd128_t src0246 = bx::simd_shuf_xyAB(bx::simd_swiz_xzxz(src0123), bx::simd_swiz_xzxz(src4567));
		bx::simd128_t src1357 = bx::simd_shuf_xyAB(bx::simd_swiz_ywyw(src0123), bx::simd_swiz_ywyw(src4567));

		bx::simd128_t dst0246 = bx::simd_add(bx::simd_add(bx::simd_mul(src0246, mtx0), bx::simd_mul(src1357, mtx2)), mtx4);
		bx::simd128_t dst1357 = bx::simd_add(bx::simd_add(bx::simd_mul(src0246, mtx1), bx::simd_mul(src1357, mtx3)), mtx5);

		bx::simd128_t dst0123 = bx::simd_swiz_xzyw(bx::simd_shuf_xyAB(dst0246, dst1357));
		bx::simd128_t dst4567 = bx::simd_swiz_xzyw(bx::simd_shuf_zwCD(dst0246, dst1357));

		_mm_storeu_ps(dst, dst0123);
		_mm_storeu_ps(dst + 4, dst4567);

		src += 8;
		dst += 8;
	}

	const uint32_t rem = n & 3;
	switch (rem) {
	case 3:
		*dst++ = mtx[0] * src[0] + mtx[2] * src[1] + mtx[4];
		*dst++ = mtx[1] * src[0] + mtx[3] * src[1] + mtx[5];
		src += 2;
	case 2:
		*dst++ = mtx[0] * src[0] + mtx[2] * src[1] + mtx[4];
		*dst++ = mtx[1] * src[0] + mtx[3] * src[1] + mtx[5];
		src += 2;
	case 1:
		*dst++ = mtx[0] * src[0] + mtx[2] * src[1] + mtx[4];
		*dst++ = mtx[1] * src[0] + mtx[3] * src[1] + mtx[5];
		src += 2;
	}
#else
	for (uint32_t i = 0; i < n; ++i) {
		const uint32_t id = i << 1;
		transformPos2D(v[id], v[id + 1], mtx, &p[id]);
	}
#endif
}

// NOTE: Assumes src is 16-byte aligned. Don't care about dst (unaligned stores)
void batchTransformDrawIndices(const uint16_t* __restrict src, uint32_t n, uint16_t* __restrict dst, uint16_t delta)
{
	if (delta == 0) {
		bx::memCopy(dst, src, sizeof(uint16_t) * n);
		return;
	}

#if VG_CONFIG_ENABLE_SIMD && BX_CPU_X86
	const __m128i xmm_delta = _mm_set1_epi16(delta);

	const uint32_t iter32 = n >> 5;
	for (uint32_t i = 0; i < iter32; ++i) {
		const __m128i s0 = _mm_load_si128((const __m128i*)src);
		const __m128i s1 = _mm_load_si128((const __m128i*)(src + 8));
		const __m128i s2 = _mm_load_si128((const __m128i*)(src + 16));
		const __m128i s3 = _mm_load_si128((const __m128i*)(src + 24));

		const __m128i d0 = _mm_add_epi16(s0, xmm_delta);
		const __m128i d1 = _mm_add_epi16(s1, xmm_delta);
		const __m128i d2 = _mm_add_epi16(s2, xmm_delta);
		const __m128i d3 = _mm_add_epi16(s3, xmm_delta);

		// NOTE: Proper alignment of dst buffer isn't guaranteed because it's part of the global IndexBuffer.
		_mm_storeu_si128((__m128i*)dst, d0);
		_mm_storeu_si128((__m128i*)(dst + 8), d1);
		_mm_storeu_si128((__m128i*)(dst + 16), d2);
		_mm_storeu_si128((__m128i*)(dst + 24), d3);

		src += 32;
		dst += 32;
	}

	uint32_t rem = n & 31;
	if (rem >= 16) {
		const __m128i s0 = _mm_load_si128((const __m128i*)src);
		const __m128i s1 = _mm_load_si128((const __m128i*)(src + 8));

		const __m128i d0 = _mm_add_epi16(s0, xmm_delta);
		const __m128i d1 = _mm_add_epi16(s1, xmm_delta);

		_mm_storeu_si128((__m128i*)dst, d0);
		_mm_storeu_si128((__m128i*)(dst + 8), d1);

		src += 16;
		dst += 16;
		rem -= 16;
	}

	if (rem >= 8) {
		__m128i s0 = _mm_load_si128((const __m128i*)src);
		__m128i d0 = _mm_add_epi16(s0, xmm_delta);
		_mm_storeu_si128((__m128i*)dst, d0);

		src += 8;
		dst += 8;
		rem -= 8;
	}

	switch (rem) {
	case 7: *dst++ = *src++ + delta;
	case 6: *dst++ = *src++ + delta;
	case 5: *dst++ = *src++ + delta;
	case 4: *dst++ = *src++ + delta;
	case 3: *dst++ = *src++ + delta;
	case 2: *dst++ = *src++ + delta;
	case 1: *dst = *src + delta;
	}
#else
	for (uint32_t i = 0; i < n; ++i) {
		*dst++ = *src + delta;
		src++;
	}
#endif
}

void convertA8_to_RGBA8(uint32_t* rgba, const uint8_t* a8, uint32_t w, uint32_t h, uint32_t rgbColor)
{
	const uint32_t rgb0 = rgbColor & 0x00FFFFFF;

	int numPixels = w * h;
	for (int i = 0; i < numPixels; ++i) {
		*rgba++ = rgb0 | (((uint32_t)* a8) << 24);
		++a8;
	}
}
}
