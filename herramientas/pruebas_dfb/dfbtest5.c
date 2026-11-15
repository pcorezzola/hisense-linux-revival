/*
 * dfbtest5.c - la forma correcta: crear una VENTANA gestionada por SaWMan.
 *
 * Contexto de lo aprendido con dfbtest1..4 en este TV:
 *  - DirectFBInit / DirectFBCreate / GetDisplayLayer / SetCooperativeLevel: OK
 *  - Lock() sobre la superficie de la capa primaria: FALLO (SaWMan es dueño de la capa)
 *  => la via es dfb->CreateWindow(): SaWMan compone la ventana sobre la interfaz actual,
 *     sin robarle la capa al launcher.
 *
 * Pinta 3 franjas en una ventana de 640x360, hace Flip, espera 5 s y sale.
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
    IDirectFBWindow *win = 0;
    IDirectFBSurface *surf = 0;
    DFBWindowDescription desc;
    DFBResult r;
    int w = 0, h = 0, pitch = 0;
    void *ptr = 0;

    static char *argvx[2] = { (char *)"dfbtest5", 0 };
    char **argvp = argvx;
    int argcx = 1;

    print("=== dfbtest5 (ventana) ===\n");

    paso("1) DirectFBInit     ", DirectFBInit(&argcx, &argvp));
    paso("2) DirectFBCreate    ", DirectFBCreate(&dfb));
    if (!dfb) { sys1_(SYS_exit, 1); }

    desc.flags = DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT |
                 DWDESC_CAPS | DWDESC_PIXELFORMAT;
    desc.posx = 200;
    desc.posy = 150;
    desc.width = 640;
    desc.height = 360;
    desc.caps = DWCAPS_NODECORATION | DWCAPS_DOUBLEBUFFER | DWCAPS_ALPHACHANNEL;
    desc.pixelformat = DSPF_ARGB;

    paso("3) GetDisplayLayer   ", dfb->GetDisplayLayer(dfb, DLID_PRIMARY, &layer));
    if (!layer) { dfb->Release(dfb); sys1_(SYS_exit, 1); }

    paso("3b) SetCooperative   ", layer->SetCooperativeLevel(layer, DLSCL_ADMINISTRATIVE));

    paso("3c) CreateWindow     ", layer->CreateWindow(layer, &desc, &win));
    print("   win="); if (win) { print("no nulo\n"); } else { print("NULO\n"); }
    if (!win) { layer->Release(layer); dfb->Release(dfb); sys1_(SYS_exit, 1); }

    paso("4) Window GetSurface ", win->GetSurface(win, &surf));
    print("   surf="); if (surf) { print("no nulo\n"); } else { print("NULO\n"); }
    if (!surf) { win->Release(win); layer->Release(layer); dfb->Release(dfb); sys1_(SYS_exit, 1); }

    surf->GetSize(surf, &w, &h);
    print("   ventana: "); printnum((unsigned int)w); print("x"); printnum((unsigned int)h); print("\n");

    r = surf->Lock(surf, DSLF_WRITE, &ptr, &pitch);
    paso("5) Lock              ", r);
    if (!r && ptr) {
        int y, x;
        unsigned int *fila;
        print("   pitch="); printnum((unsigned int)pitch); print("\n");
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
        paso("6) Unlock            ", surf->Unlock(surf));
        paso("7) Flip              ", surf->Flip(surf, 0, DSFLIP_WAITFORSYNC));
        print("8) en pantalla 5 s...\n");
        ms(5000);
    }

    surf->Release(surf);
    win->Release(win);
    layer->Release(layer);
    dfb->Release(dfb);

    print("=== fin ===\n");
    sys1_(SYS_exit, 0);
    return 0;
}


