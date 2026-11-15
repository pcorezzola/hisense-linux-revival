/*
 * dfbtest7.c - replica exacta y minima de lo que hace skype_ap en este TV:
 *   DirectFBInit -> DirectFBCreate -> GetDisplayLayer -> GetSurface ->
 *   SetColor -> FillRectangle -> Flip
 *
 * Sin SetCooperativeLevel, sin SetConfiguration y sin Lock: se imprime el resultado de CADA
 * llamada antes de la siguiente, asi si algo mata el proceso sabemos exactamente donde.
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
    if (r) { print(" FALLO="); printnum((unsigned int)r); print("\n"); }
    else { print(" ok\n"); }
}

int main(void)
{
    IDirectFB *dfb = 0;
    IDirectFBDisplayLayer *layer = 0;
    IDirectFBSurface *surf = 0;
    DFBResult r;
    int w = 0, h = 0;
    DFBSurfacePixelFormat fmt;
    static char *argvx[2] = { (char *)"dfbtest7", 0 };
    char **argvp = argvx;
    int argcx = 1;

    print("=== dfbtest7 (secuencia de skype_ap) ===\n");

    paso("1 Init        ", DirectFBInit(&argcx, &argvp));
    paso("2 Create      ", DirectFBCreate(&dfb));
    if (!dfb) sys1_(SYS_exit, 1);
    paso("3 GetLayer    ", dfb->GetDisplayLayer(dfb, DLID_PRIMARY, &layer));
    if (!layer) { dfb->Release(dfb); sys1_(SYS_exit, 1); }
    paso("4 GetSurface  ", layer->GetSurface(layer, &surf));
    if (!surf) { layer->Release(layer); dfb->Release(dfb); sys1_(SYS_exit, 1); }

    surf->GetSize(surf, &w, &h);
    surf->GetPixelFormat(surf, &fmt);
    print("  superficie "); printnum((unsigned int)w); print("x"); printnum((unsigned int)h);
    print(" fmt="); printnum((unsigned int)fmt); print("\n");

    paso("5 SetColor    ", surf->SetColor(surf, 0xFF, 0x00, 0x00, 0xFF));
    paso("6 FillRect    ", surf->FillRectangle(surf, 0, 0, w, h));
    paso("7 Flip        ", surf->Flip(surf, 0, DSFLIP_WAITFORSYNC));
    print("8 en pantalla 4 s...\n");
    ms(4000);

    surf->Release(surf);
    layer->Release(layer);
    dfb->Release(dfb);
    print("=== fin ===\n");
    sys1_(SYS_exit, 0);
    return 0;
}
