#include "clt13.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

int g_verbose = 1;

int expect(char * desc, int expected, int recieved);

#ifdef ENABLE_EXTENDED_API
int main(void)
{
    puts("TEST_CLT not compatible with extended encoding api");
    return(1);
}
#else
int main(void)
{
#ifdef DISABLE_CRT_TREE
    puts("CRT TREE DISABLED");
#else
    puts("CRT TREE ENABLED");
#endif
    srand(time(NULL));

    ulong nzs     = 10;
    ulong lambda  = 30;
    ulong kappa   = 2;

    clt_state mmap, mmap_;
    clt_pp pp_, pp;

    int pows [nzs];
    for (ulong i = 0; i < nzs; i++) pows[i] = 1;

    // make test directories
    const char *mmap_dir = "test.mmap";
    const char *pp_dir   = "test.pp";
    if (mkdir(mmap_dir, S_IRWXU | S_IRWXG) != 0) {
        if (errno != EEXIST) {
            fprintf(stderr, "couldn't make dir \"%s\"", mmap_dir);
            return -1;
        }
    }
    if (mkdir(pp_dir, S_IRWXU | S_IRWXG) != 0) {
        if (errno != EEXIST) {
            fprintf(stderr, "couldn't make dir \"%s\"", pp_dir);
            return -1;
        }
    }

    // test initialization & serialization
    clt_state_init(&mmap_, kappa, lambda, nzs, pows);
    clt_state_save(&mmap_, mmap_dir);
    clt_state_clear(&mmap_);
    clt_state_read(&mmap, mmap_dir);

    clt_pp_init(&pp_, &mmap);
    clt_pp_save(&pp_, pp_dir);
    clt_pp_clear(&pp_);
    clt_pp_read(&pp, pp_dir);

    mpz_t x [1];
    mpz_init_set_ui(x[0], 0);
    while (mpz_cmp_ui(x[0], 0) <= 0) {
        mpz_set_ui(x[0], rand());
        mpz_mod(x[0], x[0], mmap.gs[0]);
    }
    gmp_printf("x = %Zd\n", x[0]);

    mpz_t zero [1];
    mpz_init_set_ui(zero[0], 0);

    mpz_t one [1];
    mpz_init_set_ui(one[0], 1);

    int top_level [nzs];
    for (ulong i = 0; i < nzs; i++) {
        top_level[i] = 1;
    }

    mpz_t x0, x1, xp;
    mpz_inits(x0, x1, xp, NULL);
    clt_encode(x0, &mmap, 1, zero, top_level);
    clt_encode(x1, &mmap, 1, zero, top_level);
    mpz_add(xp, x0, x1);
    mpz_mod(xp, xp, mmap.x0);
    int ok = expect("is_zero(0 + 0)", 1, clt_is_zero(&pp, xp));

    clt_encode(x0, &mmap, 1, zero, top_level);
    clt_encode(x1, &mmap, 1, one,  top_level);
    mpz_add(xp, x0, x1);
    mpz_mod(xp, xp, mmap.x0);
    ok &= expect("is_zero(0 + 1)", 0, clt_is_zero(&pp, xp));

    clt_encode(x0, &mmap, 1, zero, top_level);
    clt_encode(x1, &mmap, 1, x,    top_level);
    mpz_add(xp, x0, x1);
    mpz_mod(xp, xp, mmap.x0);
    ok &= expect("is_zero(0 + x)", 0, clt_is_zero(&pp, xp));

    clt_encode(x0, &mmap, 1, x, top_level);
    clt_encode(x1, &mmap, 1, x, top_level);
    mpz_sub(xp, x0, x1);
    mpz_mod(xp, xp, mmap.x0);
    ok &= expect("is_zero(x - x)", 1, clt_is_zero(&pp, xp));

    clt_encode(x0, &mmap, 1, zero, top_level);
    clt_encode(x1, &mmap, 1, x,    top_level);
    mpz_sub(xp, x0, x1);
    mpz_mod(xp, xp, mmap.x0);
    ok &= expect("is_zero(0 - x)", 0, clt_is_zero(&pp, xp));

    clt_encode(x0, &mmap, 1, one,  top_level);
    clt_encode(x1, &mmap, 1, zero, top_level);
    mpz_sub(xp, x0, x1);
    mpz_mod(xp, xp, mmap.x0);
    ok &= expect("is_zero(1 - 0)", 0, clt_is_zero(&pp, xp));

    int ix0 [nzs];
    int ix1 [nzs];
    for (ulong i = 0; i < nzs; i++) {
        if (i < nzs / 2) {
            ix0[i] = 1;
            ix1[i] = 0;
        } else {
            ix0[i] = 0;
            ix1[i] = 1;
        }
    }
    clt_encode(x0, &mmap, 1, x   , ix0);
    clt_encode(x1, &mmap, 1, zero, ix1);
    mpz_mul(xp, x0, x1);
    mpz_mod(xp, xp, mmap.x0);
    ok &= expect("is_zero(x * 0)", 1, clt_is_zero(&pp, xp));

    clt_encode(x0, &mmap, 1, x  , ix0);
    clt_encode(x1, &mmap, 1, one, ix1);
    mpz_mul(xp, x0, x1);
    mpz_mod(xp, xp, mmap.x0);
    ok &= expect("is_zero(x * 1)", 0, clt_is_zero(&pp, xp));

    clt_encode(x0, &mmap, 1, x, ix0);
    clt_encode(x1, &mmap, 1, x, ix1);
    mpz_mul(xp, x0, x1);
    mpz_mod(xp, xp, mmap.x0);
    ok &= expect("is_zero(x * x)", 0, clt_is_zero(&pp, xp));

    // zimmerman-like test

    mpz_t c;
    mpz_t in0 [2];
    mpz_t in1 [2];
    mpz_t cin [2];

    mpz_inits(c, in0[0], in0[1], in1[0], in1[1], cin[0], cin[1], NULL);

    mpz_urandomb(in1[0], mmap.rng, lambda);
    mpz_mod(in1[0], in1[0], mmap.gs[0]);

    mpz_set_ui(in0[0], 0);
    mpz_set_ui(cin[0], 0);

    mpz_urandomb(in0[1], mmap.rng, 16);
    mpz_urandomb(in1[1], mmap.rng, 16);
    mpz_mul(cin[1], in0[1], in1[1]);

    clt_encode(x0, &mmap, 2, in0, ix0);
    clt_encode(x1, &mmap, 2, in1, ix1);
    clt_encode(c,  &mmap, 2, cin, top_level);

    mpz_mul(xp, x0, x1);
    mpz_mod(xp, xp, mmap.x0);

    mpz_sub(xp, xp, c);
    mpz_mod(xp, xp, mmap.x0);

    ok &= expect("[Z] is_zero(0 * x)", 1, clt_is_zero(&pp, xp));

    mpz_set_ui(in0[0], 1);
    mpz_set_ui(in1[0], 1);
    mpz_set_ui(cin[0], 0);

    mpz_urandomb(in0[0], mmap.rng, lambda);
    mpz_mod(in0[0], in0[0], mmap.gs[0]);

    mpz_urandomb(in1[0], mmap.rng, lambda);
    mpz_mod(in1[0], in1[0], mmap.gs[0]);

    mpz_urandomb(in0[1], mmap.rng, 16);
    mpz_urandomb(in1[1], mmap.rng, 16);
    mpz_mul(cin[1], in0[1], in1[1]);

    clt_encode(x0, &mmap, 2, in0, ix0);
    clt_encode(x1, &mmap, 2, in1, ix1);
    clt_encode(c,  &mmap, 2, cin, top_level);

    mpz_mul(xp, x0, x1);
    mpz_mod(xp, xp, mmap.x0);

    mpz_sub(xp, xp, c);
    mpz_mod(xp, xp, mmap.x0);

    ok &= expect("[Z] is_zero(x * y)", 0, clt_is_zero(&pp, xp));
    clt_state_clear(&mmap);
    clt_pp_clear(&pp);
    mpz_clears(c, x0, x1, xp, x[0], zero[0], one[0], in0[0], in0[1], in1[0], in1[1], cin[0], cin[1], NULL);
    return !ok;
}
#endif

int expect(char * desc, int expected, int recieved)
{
    if (expected != recieved) {
        printf("\033[1;41m");
    }
    printf("%s = %d", desc, recieved);
    if (expected != recieved) {
        printf("\033[0m");
    }
    puts("");
    return expected == recieved;
}