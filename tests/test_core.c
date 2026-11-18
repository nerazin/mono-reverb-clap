/* Regression test for mono_reverb.h that needs no firmware.
 *
 * The expected hashes come from Monomachine OS 1.32B itself: the tables were
 * read from the emulated DSP's memory, and the same stimulus was fed into the
 * firmware's REVERB machine running in an emulator (see the README). The
 * stimulus starts with the machine's silent 256 blocks, moves every control,
 * including to the ends of their range, drives the input to full scale and
 * leaves gaps for the tail and the gate. */
#include <stdio.h>
#include "../source/mono_reverb.h"

static uint32_t seed = 1;
static uint32_t rnd(uint32_t n) { seed = seed * 1664525u + 1013904223u; return (seed >> 8) % n; }

/* Controls and input of one block. */
static void stimulus(int block, int32_t ctl[MR_CONTROLS], int32_t in[2][MR_FRAMES]) {
   static int level = 8;
   static const int32_t defaults[MR_CONTROLS] = {64, 2, 100, 64, 0, 127, 64};
   if (block == 0)
      for (int k = 0; k < MR_CONTROLS; ++k) ctl[k] = defaults[k] << 16;
   else if (block % 61 == 0) {                      /* knob-like moves, also between steps and to the ends */
      const int k = (int)rnd(MR_CONTROLS);
      ctl[k] = rnd(4) == 0 ? (rnd(2) ? 0x7fffff : 0) : (int32_t)rnd(0x7f0000);
      level = 8 + (int)rnd(8);                      /* full scale down to -42 dB */
   }
   for (int c = 0; c < 2; ++c)
      for (int n = 0; n < MR_FRAMES; ++n) {
         seed = seed * 1664525u + 1013904223u;
         in[c][n] = block % 400 < 300 ? (int32_t)seed >> level : 0;
      }
}

static uint64_t hash = 1469598103934665603ULL;
static void mix(const int32_t *v, int n) {
   for (int i = 0; i < n; ++i)
      for (int b = 0; b < 4; ++b) hash = (hash ^ (((uint32_t)v[i] >> (8 * b)) & 0xff)) * 1099511628211ULL;
}

#define TEST_BLOCKS 12000

#ifndef TEST_CORE_NO_MAIN
int main(void) {
   static MonoReverb p;
   mr_tables();
   mr_reset(&p);
   mix(&mr_dec[0][0], 64 * MR_LINES); mix(&mr_damp[0][0], 64 * MR_LINES); mix(mr_coef, 128); mix(mr_lfo, 512);
   const uint64_t tables = hash;
   hash = 1469598103934665603ULL;
   int32_t ctl[MR_CONTROLS], in[2][MR_FRAMES], out[2][MR_FRAMES];
   for (int block = 0; block < TEST_BLOCKS; ++block) {
      stimulus(block, ctl, in);
      mr_block(&p, ctl, in, out);
      mix(out[0], MR_FRAMES); mix(out[1], MR_FRAMES);
   }
   const uint64_t want_tables = 0xa627709b57e7f60cULL, want_audio = 0x9f8008641a91c751ULL;
   printf("tables %016llx audio %016llx\n", (unsigned long long)tables, (unsigned long long)hash);
   if (tables != want_tables || hash != want_audio) { puts("FAIL: output differs from the firmware"); return 1; }
   puts("PASS core tables and audio match the firmware");
   return 0;
}
#endif
