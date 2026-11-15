/*
 * dfbtest4.c - DirectFB con nivel de cooperacion y dibujo por Lock (no por SetColor/Fill).
 *
 * Lo que aprendimos:
 *  - DirectFBInit y DirectFBCreate andan sin tocar la interfaz (core multi-aplicacion).
 *  - GetDisplayLayer anda, pero sin SetCooperativeLevel la superficie queda "unprivileged"
 *    y el primer SetColor mata el proceso (SIGSEGV), aunque el TV sigue vivo.
 *  - La capa primaria es 1920x1080 (no 1280x720): SetConfiguration con 720p fue rechazado.
 *
 * Estrategia: pedir DLSCL_ADMINISTRATIVE, usar la superficie que da la capa tal cual,
 * y pintar con Lock/Unlock (puntero + pitch) que es el camino mas directo y verificable.
 */

#include <directfb.h>

#define SYS_exit      1
#define SYS_write     4
#define SYS_nanosleep 162

static long sys3_(long n, long a, long b, long c)
{
    register long r7 __asm__("r7") = n;
    register long r0 __asm__("r0") = a;
    register long r1 __asm__("r1") = b;
    register long r2 __asm__("r2") = c;
    __asm__ volatile ("svc 0" : "+r"(r0) : "r"(r7), "r"(r1), "r"(r2) : "memory", "cc");
    return r0;
}
#define sys1_(n,a)   sys3_(n,a,0,0)
#define sys2_(n,a,b) sys3_(n,a,b,0)

static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static void print(const char *s) { sys3_(SYS_write, 1, (long)s, slen(s)); }

static void printnum(unsigned int v)
{
    char b[12], o[12];
    int i = 0, j = 0;
    if (!v) b[i++] = '0';
    while (v) { unsigned int d = v; int k = 0; while (d >= 10) { d -= 10; k++; } b[i++] = (char)('0' + (v - k * 10)); v = k; }
    while (i) o[j++] = b[--i];
    o[j] = 0;
    print(o);
}

static void ms(int m)
{
    long ts[2];
    ts[0] = m / 1000;
    ts[1] = (long)(m - ts[0] * 1000) * 1000000L;
    sys2_(SYS_nanosleep, (long)ts, 0);
}

static void paso(const char *que, DFBResult r)
{
    print(que);
    if (r) { print(" FALLO codigo="); printnum((unsigned int)r); print("\n"); }
    else { print(" ok\n"); }
}

int main(void)
{
    IDirectFB *dfb = 0;
    IDirectFBDisplayLayer *layer = 0;
    IDirectFBSurface *surf = 0;
    DFBResult r;
    int w = 0, h = 0, pitch = 0;
    void *ptr = 0;
    DFBSurfacePixelFormat fmt;

    static char *argvx[2] = { (char *)"dfbtest4", 0 };
    char **argvp = argvx;
    int argcx = 1;

    print("=== dfbtest4 ===\n");

    paso("1) DirectFBInit     ", DirectFBInit(&argcx, &argvp));
    paso("2) DirectFBCreate    ", DirectFBCreate(&dfb));
    if (!dfb) { sys1_(SYS_exit, 1); }

    paso("3) GetDisplayLayer   ", dfb->GetDisplayLayer(dfb, DLID_PRIMARY, &layer));
    if (!layer) { dfb->Release(dfb); sys1_(SYS_exit, 1); }

    paso("4) SetCooperative    ", layer->SetCooperativeLevel(layer, DLSCL_ADMINISTRATIVE));

    paso("5) GetSurface        ", layer->GetSurface(layer, &surf));
    if (!surf) { layer->Release(layer); dfb->Release(dfb); sys1_(SYS_exit, 1); }

    surf->GetSize(surf, &w, &h);
    surf->GetPixelFormat(surf, &fmt);
    print("   superficie: "); printnum((unsigned int)w); print("x"); printnum((unsigned int)h);
    print(" fmt="); printnum((unsigned int)fmt); print("\n");

    r = surf->Lock(surf, DSLF_WRITE, &ptr, &pitch);
    paso("6) Lock              ", r);
    if (!r && ptr) {
        print("   ptr no nulo, pitch="); printnum((unsigned int)pitch); print("\n");
        /* pintar 3 franjas horizontales: rojo, verde, azul (ARGB 32bpp) */
        {
            int y, x;
            unsigned int *fila;
            for (y = 0; y < h; y++) {
                fila = (unsigned int *)((char *)ptr + y * pitch);
                for (x = 0; x < w; x++) {
                    unsigned int c;
                    if (y < h / 3) { c = 0xFFFF0000u; }
                    else if (y < (2 * h) / 3) { c = 0xFF00FF00u; }
                    else { c = 0xFF0000FFu; }
                    fila[x] = c;
                }
            }
        }
        paso("7) Unlock            ", surf->Unlock(surf));
        paso("8) Flip              ", surf->Flip(surf, 0, DSFLIP_WAITFORSYNC));
        print("9) en pantalla 5 s...\n");
        ms(5000);
    }

    surf->Release(surf);
    layer->Release(layer);
    dfb->Release(dfb);

    print("=== fin ===\n");
    sys1_(SYS_exit, 0);
    return 0;
}
