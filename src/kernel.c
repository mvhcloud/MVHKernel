#include <stdint.h>
#include "mvh/cpu.h"
#include "mvh/io.h"
#include "mvh/keyboard.h"
#include "mvh/serial.h"
#include "mvh/vga.h"

static uint64_t total_memory_kib;
static uint8_t language;

extern char __kernel_end;

static void console_put(char value)
{
    vga_put(value);
    serial_put(value);
}

static void console_write(const char *text)
{
    while (*text != '\0') {
        console_put(*text++);
    }
}

static void console_number(uint64_t value)
{
    char digits[21];
    unsigned int length = 0;
    if (value == 0u) {
        console_write("0");
        return;
    }
    while (value != 0u) {
        digits[length++] = (char)('0' + value % 10u);
        value /= 10u;
    }
    while (length != 0u) {
        console_put(digits[--length]);
    }
}

static void console_colored(const char *text, uint8_t color)
{
    uint8_t previous = vga_get_color();
    vga_set_color(color);
    console_write(text);
    vga_set_color(previous);
}

static int text_equals(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (*left != *right) {
            return 0;
        }
        left++;
        right++;
    }
    return *left == *right;
}

static const char *localized(const char *english, const char *german,
                             const char *spanish, const char *french)
{
    if (language == 1u) {
        return german;
    }
    if (language == 2u) {
        return spanish;
    }
    if (language == 3u) {
        return french;
    }
    return english;
}

static void show_prompt(void)
{
    console_colored("mvh> ", 0x0Au);
}

static void show_languages(void)
{
    console_colored("+------------------- LANGUAGE --------------------+\n", 0x0Du);
    console_write("| language en   English");
    if (language == 0u) {
        console_write("  [active]");
    }
    console_write("\n| language de   Deutsch");
    if (language == 1u) {
        console_write("  [active]");
    }
    console_write("\n| language es   Espanol");
    if (language == 2u) {
        console_write("  [active]");
    }
    console_write("\n| language fr   Francais");
    if (language == 3u) {
        console_write("  [active]");
    }
    console_write("\n");
    console_colored("+-------------------------------------------------+\n", 0x0Du);
}

static int set_language(const char *code)
{
    if (text_equals(code, "en")) {
        language = 0u;
    } else if (text_equals(code, "de")) {
        language = 1u;
    } else if (text_equals(code, "es")) {
        language = 2u;
    } else if (text_equals(code, "fr")) {
        language = 3u;
    } else {
        return 0;
    }
    return 1;
}

static const char *skip_spaces(const char *text)
{
    while (*text == ' ') {
        text++;
    }
    return text;
}

static void show_ram(void)
{
    uint64_t used_kib = ((uint64_t)(uintptr_t)&__kernel_end + 1023u) / 1024u;
    uint64_t percent;
    uint64_t filled;
    unsigned int index;
    console_colored("+---------------------- RAM ----------------------+\n", 0x0Bu);
    if (total_memory_kib == 0u) {
        console_write(localized("| Memory information is unavailable.\n",
                                "| Speicherinformation nicht verfuegbar.\n",
                                "| Informacion de memoria no disponible.\n",
                                "| Informations memoire indisponibles.\n"));
    } else {
        if (used_kib > total_memory_kib) {
            used_kib = total_memory_kib;
        }
        percent = (used_kib * 100u + total_memory_kib - 1u) / total_memory_kib;
        filled = (percent * 30u + 99u) / 100u;
        console_write("| [");
        for (index = 0; index < 30u; index++) {
            console_put(index < filled ? '#' : '-');
        }
        console_write("] ");
        console_number(percent);
        console_write(localized("%\n| Used: ", "%\n| Belegt: ",
                                "%\n| Usado: ", "%\n| Utilise: "));
        console_number((used_kib + 1023u) / 1024u);
        console_write(localized(" MiB   Free: ", " MiB   Frei: ",
                                " MiB   Libre: ", " MiB   Libre: "));
        console_number((total_memory_kib - used_kib) / 1024u);
        console_write(localized(" MiB   Total: ", " MiB   Gesamt: ",
                                " MiB   Total: ", " MiB   Total: "));
        console_number(total_memory_kib / 1024u);
        console_write(" MiB\n");
    }
    console_colored("+-------------------------------------------------+\n", 0x0Bu);
}

static void show_cpu(void)
{
    char vendor[13];
    char brand[49];
    uint32_t ecx = cpu_feature_ecx();
    uint32_t edx = cpu_feature_edx();
    uint32_t logical_cpus;
    uint32_t core;
    unsigned int bar;
    cpu_vendor(vendor);
    cpu_brand(brand);
    logical_cpus = cpu_logical_count();
    console_colored("+---------------------- CPU ----------------------+\n", 0x0Eu);
    console_write(localized("| Architecture : x86_64\n| Vendor       : ",
                            "| Architektur  : x86_64\n| Hersteller   : ",
                            "| Arquitectura : x86_64\n| Fabricante   : ",
                            "| Architecture : x86_64\n| Fabricant    : "));
    console_write(vendor);
    console_write(localized("\n| Model        : ", "\n| Modell       : ",
                            "\n| Modelo       : ", "\n| Modele       : "));
    console_write(brand[0] == '\0' ? "Unknown" : skip_spaces(brand));
    console_write(localized("\n| Logical CPUs : ", "\n| Logische CPUs: ",
                            "\n| CPUs logicas : ", "\n| CPUs logiques: "));
    console_number(logical_cpus);
    console_write(localized("\n| Features     : ", "\n| Funktionen   : ",
                            "\n| Funciones    : ", "\n| Fonctions    : "));
    if ((edx & (1u << 25u)) != 0u) {
        console_write("SSE ");
    }
    if ((edx & (1u << 26u)) != 0u) {
        console_write("SSE2 ");
    }
    if ((ecx & (1u << 28u)) != 0u) {
        console_write("AVX ");
    }
    console_write("\n");
    console_write("|\n");
    if (logical_cpus > 16u) {
        logical_cpus = 16u;
    }
    for (core = 0; core < logical_cpus; core++) {
        console_write("| Core ");
        console_number(core);
        console_write(" [");
        for (bar = 0; bar < 24u; bar++) {
            console_put(core == 0u ? '#' : '-');
        }
        console_write(core == 0u ? "] ACTIVE\n" : "] AVAILABLE\n");
    }
    console_colored("+-------------------------------------------------+\n", 0x0Eu);
}

static void show_about(void)
{
    console_colored("+==================== MVHCLOUD ====================+\n", 0x0Bu);
    console_write(localized("| Product      : MVHCLOUD OS\n", "| Produkt      : MVHCLOUD OS\n",
                            "| Producto     : MVHCLOUD OS\n", "| Produit      : MVHCLOUD OS\n"));
    console_write("| Kernel       : MVH Kernel 1.0\n");
    console_write(localized("| Architecture : x86_64 / ELF64\n", "| Architektur  : x86_64 / ELF64\n",
                            "| Arquitectura : x86_64 / ELF64\n", "| Architecture : x86_64 / ELF64\n"));
    console_write("| Website      : https://MVHCLOUD.com\n");
    console_write(localized("| Language     : English\n", "| Sprache      : Deutsch\n",
                            "| Idioma       : Espanol\n", "| Langue       : Francais\n"));
    console_write("| Keyboard     : English US QWERTY\n");
    console_write(localized("| Drivers      : VGA, PS/2, UART 16550, CPUID\n",
                            "| Treiber      : VGA, PS/2, UART 16550, CPUID\n",
                            "| Controladores: VGA, PS/2, UART 16550, CPUID\n",
                            "| Pilotes      : VGA, PS/2, UART 16550, CPUID\n"));
    console_write("| License      : MIT\n");
    console_colored("+--------------------------------------------------+\n", 0x0Bu);
    console_write(localized("MVHCLOUD builds independent software foundations\nfor custom operating systems and experiments.\n",
                            "MVHCLOUD entwickelt unabhaengige Grundlagen\nfuer eigene Betriebssysteme und Experimente.\n",
                            "MVHCLOUD crea bases de software independientes\npara sistemas operativos y experimentos.\n",
                            "MVHCLOUD cree des bases logicielles independantes\npour les systemes et les experimentations.\n"));
}

static void show_statistics(void)
{
    char input;
    vga_cursor_disable();
    vga_clear();
    console_colored("+==================================================+\n", 0x0Bu);
    console_colored("|          MVHCLOUD SYSTEM STATISTICS              |\n", 0x0Fu);
    console_colored("+==================================================+\n\n", 0x0Bu);
    show_ram();
    console_write("\n");
    show_cpu();
    console_colored(localized("\nPress Q or Ctrl+C to return to the shell.\n",
                              "\nQ oder Strg+C druecken, um zur Shell zurueckzukehren.\n",
                              "\nPulsa Q o Ctrl+C para volver a la consola.\n",
                              "\nAppuyez sur Q ou Ctrl+C pour revenir au terminal.\n"), 0x0Au);
    for (;;) {
        input = keyboard_read_char();
        if (input == 'q' || input == 'Q' || input == 3) {
            break;
        }
    }
    vga_clear();
    vga_cursor_enable();
}

static void reboot_wait(void)
{
    volatile uint64_t delay;
    for (delay = 0; delay < 300000u; delay++) {
        __asm__ volatile ("pause");
    }
}

static void reboot_step(const char *message)
{
    unsigned int dot;
    console_write(message);
    for (dot = 0; dot < 5u; dot++) {
        reboot_wait();
        console_colored(".", 0x0Eu);
    }
    console_colored(" [OK]\n", 0x0Au);
}

static void reboot(void)
{
    console_colored("\n+=============== MVHCLOUD REBOOT ===============+\n", 0x0Cu);
    console_colored("+------------------------------------------------+\n", 0x0Cu);
    reboot_step(localized("Locking kernel shell      ", "Kernel-Shell sperren     ",
                          "Bloqueando consola        ", "Verrouillage du terminal "));
    reboot_step(localized("Stopping keyboard input   ", "Tastatureingabe stoppen  ",
                          "Deteniendo teclado        ", "Arret du clavier         "));
    __asm__ volatile ("cli");
    reboot_step(localized("Disabling CPU interrupts  ", "CPU-Interrupts aus       ",
                          "Desactivando interrupciones", "Arret des interruptions  "));
    reboot_step(localized("Preparing reset controller", "Reset-Controller bereit  ",
                          "Preparando controlador    ", "Preparation du controleur"));
    while ((io_in8(0x64u) & 2u) != 0u) {
    }
    console_colored(localized("Sending hardware reset", "Hardware-Reset senden",
                              "Enviando reinicio", "Envoi du redemarrage"), 0x0Fu);
    console_colored(".....\n", 0x0Eu);
    io_out8(0x64u, 0xFEu);
    console_colored(localized("Reset failed. CPU halted.\n", "Reset fehlgeschlagen. CPU angehalten.\n",
                              "Fallo de reinicio. CPU detenida.\n",
                              "Echec du redemarrage. CPU arretee.\n"), 0x0Cu);
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

static void run_command(const char *command)
{
    if (text_equals(command, "help")) {
        console_colored(localized("Available commands\n", "Verfuegbare Befehle\n",
                                  "Comandos disponibles\n", "Commandes disponibles\n"), 0x0Au);
        console_write(localized(
            "  help         Show this command list\n  about        Show system information\n  statics      Open CPU and RAM monitor\n  language     Show or change language\n  clear        Clear the screen\n  reboot       Restart the system\n",
            "  help         Diese Befehlsliste anzeigen\n  about        Systeminformationen anzeigen\n  statics      CPU- und RAM-Monitor oeffnen\n  language     Sprache anzeigen oder wechseln\n  clear        Bildschirm leeren\n  reboot       System neu starten\n",
            "  help         Mostrar esta lista\n  about        Mostrar informacion del sistema\n  statics      Abrir monitor de CPU y RAM\n  language     Mostrar o cambiar idioma\n  clear        Limpiar la pantalla\n  reboot       Reiniciar el sistema\n",
            "  help         Afficher cette liste\n  about        Afficher les informations systeme\n  statics      Ouvrir le moniteur CPU et RAM\n  language     Afficher ou changer la langue\n  clear        Effacer l'ecran\n  reboot       Redemarrer le systeme\n"));
    } else if (text_equals(command, "language")) {
        show_languages();
    } else if (text_equals(command, "language en") || text_equals(command, "language de") ||
               text_equals(command, "language es") || text_equals(command, "language fr")) {
        set_language(command + 9);
        console_write(localized("Language changed to English.\n", "Sprache auf Deutsch geaendert.\n",
                                "Idioma cambiado a Espanol.\n", "Langue changee en Francais.\n"));
    } else if (text_equals(command, "clear")) {
        vga_clear();
        console_write(localized("Screen cleared.\n", "Bildschirm geleert.\n",
                                "Pantalla limpiada.\n", "Ecran efface.\n"));
    } else if (text_equals(command, "about")) {
        show_about();
    } else if (text_equals(command, "statics")) {
        show_statistics();
    } else if (text_equals(command, "reboot")) {
        reboot();
    } else if (command[0] != '\0') {
        console_write(localized("Unknown command. Type 'help' for a list.\n",
                                "Unbekannter Befehl. 'help' zeigt die Liste.\n",
                                "Comando desconocido. Escribe 'help'.\n",
                                "Commande inconnue. Tapez 'help'.\n"));
    }
}

void kernel_main(uint64_t memory_kib, uint64_t boot_data)
{
    char command[64];
    unsigned int length = 0;
    char input;
    (void)boot_data;
    total_memory_kib = memory_kib;
    serial_init();
    vga_init();
    console_colored("+==================================================+\n", 0x0Bu);
    console_colored("|              MVHCLOUD OS x86_64                  |\n", 0x0Fu);
    console_colored("|          Version 1.0 - MVHCLOUD.com              |\n", 0x0Fu);
    console_colored("+==================================================+\n", 0x0Bu);
    console_write("MVH kernel ready\n");
    console_write("\nType 'help' for available commands.\n");
    console_write("System monitor: statics\n\n");
    show_prompt();
    for (;;) {
        input = keyboard_read_char();
        if (input == '\n') {
            console_put('\n');
            command[length] = '\0';
            run_command(command);
            length = 0;
            show_prompt();
        } else if (input == '\b') {
            if (length > 0u) {
                length--;
                vga_put('\b');
            }
        } else if (input == 3) {
            length = 0;
            console_write("^C\n");
            show_prompt();
        } else if (input >= ' ' && length < sizeof(command) - 1u) {
            command[length++] = input;
            console_put(input);
        }
    }
}
