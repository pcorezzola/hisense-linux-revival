/*
 * libprobe.so - prueba de persistencia para TV Hisense MT5882 (Linux 3.10, armv7l soft-float)
 *
 * Objetivo: comprobar si el launcher (Presto 2.12) ejecuta nuestro userjs al arrancar y si
 * ese userjs puede cargar una .so propia via Hisense.loadLibrary(). Si ambas cosas son ciertas,
 * el constructor de esta .so corre como root en CADA arranque -> persistencia sin flashear.
 *
 * Sin libc: solo syscalls ARM EABI. Asi no depende de la glibc del TV (2.18) ni de simbolos
 * versionados. Perfil identico a la referencia que ya funciono en este equipo: e_flags
 * 0x05000200 (soft-float), ET_DYN, sin DT_NEEDED, constructor en .init_array.
 *
 * IMPORTANTE (bug corregido): en ARM EABI el NUMERO de syscall va en r7 y los argumentos en
 * r0..r6. Ponerlo en r0 (convencion x86/ARM OABI) hace que el kernel ejecute un syscall
 * arbitrario segun lo que hubiera en r7 -> el proceso muere con SIGILL/SIGSEGV.
 *
 * Comportamiento:
 *   1. Deja un marcador (con uid/pid) en /data y en /3rd_rw/appdata.
 *   2. Solo si existe /data/ARRANCAR_DROPBEAR intenta levantar dropbear con
 *      /data/bin/iniciar_dropbear.sh. Borrar ese archivo desactiva la parte activa.
 *
 * Con -DCTRL_VACIA compila una .so de control con constructor vacio (para distinguir
 * "el loader rechaza la .so" de "mi codigo falla").
 */

#define SYS_write   4
#define SYS_open    5
#define SYS_close   6
#define SYS_getuid 24
#define SYS_getpid 20
#define SYS_fork    2
#define SYS_setsid 66
#define SYS_execve 11
#define SYS_exit    1

#define O_RDONLY 0x0
#define O_WRONLY 0x1
#define O_CREAT  0x40
#define O_APPEND 0x400

/* numero en r7, argumentos en r0..r5 (ARM EABI) */
static long sys6(long n, long a, long b, long c, long d, long e, long f)
{
    register long r7 __asm__("r7") = n;
    register long r0 __asm__("r0") = a;
    register long r1 __asm__("r1") = b;
    register long r2 __asm__("r2") = c;
    register long r3 __asm__("r3") = d;
    register long r4 __asm__("r4") = e;
    register long r5 __asm__("r5") = f;
    __asm__ volatile ("svc 0"
                      : "+r"(r0)
                      : "r"(r7), "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5)
                      : "memory", "cc");
    return r0;
}

static long sys0(long n)                    { return sys6(n, 0, 0, 0, 0, 0, 0); }
static long sys1(long n, long a)            { return sys6(n, a, 0, 0, 0, 0, 0); }
static long sys3(long n, long a, long b, long c) { return sys6(n, a, b, c, 0, 0, 0); }

/* --- utilidades minimas de texto (sin libc) --- */

static int slen(const char *s)
{
    int n = 0;
    while (s[n])
        n++;
    return n;
}

static char *stpcpy_(char *d, const char *s)
{
    while (*s)
        *d++ = *s++;
    *d = 0;
    return d;
}

/* Sin division: ARMv7 sin unidad de division hace que gcc llame a __aeabi_uidivmod
 * (helper de libgcc que el TV no tiene cargado) y el dlopen fallaria. */
static char *utostr(char *d, unsigned int v)
{
    static const unsigned int pot10[10] = {
        1000000000u, 100000000u, 10000000u, 1000000u, 100000u,
        10000u, 1000u, 100u, 10u, 1u
    };
    int i, empezado = 0;
    for (i = 0; i < 10; i++) {
        int digito = 0;
        while (v >= pot10[i]) {
            v -= pot10[i];
            digito++;
        }
        if (digito || empezado || i == 9) {
            *d++ = (char)('0' + digito);
            empezado = 1;
        }
    }
    *d = 0;
    return d;
}

static void append(const char *path, const char *tag)
{
    long fd = sys3(SYS_open, (long)path, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0)
        return;
    char buf[224];
    char *p = buf;
    p = stpcpy_(p, tag);
    p = stpcpy_(p, " uid=");
    p = utostr(p, (unsigned int)sys0(SYS_getuid));
    p = stpcpy_(p, " pid=");
    p = utostr(p, (unsigned int)sys0(SYS_getpid));
    p = stpcpy_(p, "\n");
    sys3(SYS_write, fd, (long)buf, p - buf);
    sys1(SYS_close, fd);
}

static int existe(const char *path)
{
    long fd = sys3(SYS_open, (long)path, O_RDONLY, 0);
    if (fd < 0)
        return 0;
    sys1(SYS_close, fd);
    return 1;
}

static void arrancar_dropbear(void)
{
    long pid = sys0(SYS_fork);
    if (pid != 0)
        return;                 /* padre (o error): sigue normal */
    sys0(SYS_setsid);
    char *argv[3];
    argv[0] = (char *)"/bin/sh";
    argv[1] = (char *)"/data/bin/iniciar_dropbear.sh";
    argv[2] = (char *)0;
    sys3(SYS_execve, (long)"/bin/sh", (long)argv, 0);
    sys1(SYS_exit, 127);        /* si execve falla, el hijo sale sin tocar al padre */
}

__attribute__((constructor)) static void libprobe_init(void)
{
#ifdef CTRL_VACIA
    return;                     /* control: solo interesa si el loader la acepta */
#else
    append("/data/persist_probe.txt", "[libprobe] constructor");
    append("/3rd_rw/appdata/persist_probe.txt", "[libprobe] constructor");

    if (existe("/data/ARRANCAR_DROPBEAR")) {
        append("/data/persist_probe.txt", "[libprobe] intentando dropbear");
        arrancar_dropbear();
    }
#endif
}
