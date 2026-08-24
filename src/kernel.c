#include <stdint.h>
#include "mvh/acpi.h"
#include "mvh/apic.h"
#include "mvh/assert.h"
#include "mvh/ata.h"
#include "mvh/block.h"
#include "mvh/bootinfo.h"
#include "mvh/cpu.h"
#include "mvh/config.h"
#include "mvh/crc32.h"
#include "mvh/device.h"
#include "mvh/desktop.h"
#include "mvh/fs.h"
#include "mvh/framebuffer.h"
#include "mvh/hal.h"
#include "mvh/hpet.h"
#include "mvh/interrupt.h"
#include "mvh/log.h"
#include "mvh/memory.h"
#include "mvh/mvhfs.h"
#include "mvh/net.h"
#include "mvh/panic.h"
#include "mvh/pci.h"
#include "mvh/rtc.h"
#include "mvh/random.h"
#include "mvh/serial.h"
#include "mvh/smbios.h"
#include "mvh/sync.h"
#include "mvh/task.h"
#include "mvh/vfs.h"
#include "mvh/vga.h"
#include "mvh/version.h"

static uint8_t language;
static mvhfs_t persistent_filesystem;
static uint8_t persistent_file_buffer[MVHFS_FILE_MAX];
static mvhfs_entry_t persistent_entries[MVHFS_MAX_FILES];

_Static_assert(MVH_CONFIG_ARCH_X86_64 == 1, "MVH Kernel requires the x86_64 config");
_Static_assert(MVH_CONFIG_BOOTINFO_V2 == 1, "BootInfo V2 must be enabled for ABI 2");
_Static_assert(MVH_CONFIG_ACPI_PARSER == 1, "ACPI parser is required for this release");
_Static_assert(MVH_CONFIG_SMBIOS_PARSER == 1, "SMBIOS parser is required for this release");

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

static int boot_option_present(const char *option)
{
    const mvh_bootinfo_snapshot_t *boot = bootinfo_current();
    const char *command;
    uint32_t option_size = 0u;
    uint32_t offset;
    if ((boot->flags & MVH_BOOTINFO_FLAG_COMMAND_LINE) == 0u || option == 0) return 0;
    while (option[option_size] != '\0') option_size++;
    command = (const char *)(uintptr_t)boot->command_line_address;
    for (offset = 0u; offset + option_size <= boot->command_line_size; offset++) {
        uint32_t index;
        if (offset != 0u && command[offset - 1u] != ' ') continue;
        for (index = 0u; index < option_size; index++)
            if (command[offset + index] != option[index]) break;
        if (index == option_size &&
            (offset + option_size == boot->command_line_size ||
             command[offset + option_size] == ' ')) return 1;
    }
    return 0;
}

static const char *command_argument(const char *command, const char *name)
{
    while (*name != '\0') {
        if (*command++ != *name++) {
            return 0;
        }
    }
    if (*command == '\0') {
        return command;
    }
    if (*command != ' ') {
        return 0;
    }
    while (*command == ' ') {
        command++;
    }
    return command;
}

static int split_path_text(const char *input, char *path, uint32_t capacity,
                           const char **text)
{
    uint32_t length = 0;
    while (*input == ' ') {
        input++;
    }
    while (*input != '\0' && *input != ' ') {
        if (length + 1u >= capacity) {
            return -1;
        }
        path[length++] = *input++;
    }
    path[length] = '\0';
    while (*input == ' ') {
        input++;
    }
    if (length == 0u || *input == '\0') {
        return -1;
    }
    *text = input;
    return 0;
}

static void console_hex16(uint16_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    console_put(digits[(value >> 12u) & 0x0Fu]);
    console_put(digits[(value >> 8u) & 0x0Fu]);
    console_put(digits[(value >> 4u) & 0x0Fu]);
    console_put(digits[value & 0x0Fu]);
}

static void console_hex64(uint64_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    int shift;
    console_write("0x");
    for (shift = 60; shift >= 0; shift -= 4) {
        console_put(digits[(value >> (uint32_t)shift) & 0x0Fu]);
    }
}

static void console_hex32(uint32_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    int shift;
    console_write("0x");
    for (shift = 28; shift >= 0; shift -= 4) {
        console_put(digits[(value >> (uint32_t)shift) & 0x0Fu]);
    }
}

static void console_two_digits(uint8_t value)
{
    console_put((char)('0' + value / 10u));
    console_put((char)('0' + value % 10u));
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

static int parse_number(const char *text, uint64_t *value)
{
    uint64_t result = 0u;
    if (*text == '\0') {
        return 0;
    }
    while (*text != '\0') {
        if (*text < '0' || *text > '9') {
            return 0;
        }
        if (result > 60000u) {
            return 0;
        }
        result = result * 10u + (uint64_t)(*text - '0');
        if (result > 60000u) {
            return 0;
        }
        text++;
    }
    *value = result;
    return 1;
}

static void show_ram(void)
{
    pmm_stats_t stats;
    uint64_t total_kib;
    uint64_t used_kib;
    uint64_t percent;
    uint64_t filled;
    unsigned int index;
    pmm_get_stats(&stats);
    total_kib = stats.total_pages * 4u;
    used_kib = stats.used_pages * 4u;
    console_colored("+---------------------- RAM ----------------------+\n", 0x0Bu);
    if (total_kib == 0u) {
        console_write(localized("| Memory information is unavailable.\n",
                                "| Speicherinformation nicht verfuegbar.\n",
                                "| Informacion de memoria no disponible.\n",
                                "| Informations memoire indisponibles.\n"));
    } else {
        if (used_kib > total_kib) {
            used_kib = total_kib;
        }
        percent = (used_kib * 100u + total_kib - 1u) / total_kib;
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
        console_number((total_kib - used_kib) / 1024u);
        console_write(localized(" MiB   Total: ", " MiB   Gesamt: ",
                                " MiB   Total: ", " MiB   Total: "));
        console_number(total_kib / 1024u);
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
    cpu_info_t info;
    cpu_capabilities_t capabilities;
    cpu_vendor(vendor);
    cpu_brand(brand);
    logical_cpus = cpu_logical_count();
    cpu_get_info(&info);
    cpu_get_capabilities(&capabilities);
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
    console_write("\n| APIC ID      : ");
    console_number(info.apic_id);
    console_write("\n| Family/Model : ");
    console_number(info.family);
    console_write("/");
    console_number(info.model);
    console_write(" stepping ");
    console_number(info.stepping);
    console_write("\n| Local APIC   : ");
    if (info.local_apic_enabled != 0u) {
        console_write(info.x2apic_enabled != 0u ? "x2APIC enabled at " : "xAPIC enabled at ");
        console_hex64(info.local_apic_base);
    } else {
        console_write(capabilities.apic != 0u ? "detected, disabled" : "not available");
    }
    console_write("\n| Temperature  : ");
    if (info.temperature_available != 0u) {
        if (info.temperature_millicelsius < 0) console_put('-');
        console_number((uint64_t)(info.temperature_millicelsius < 0 ?
                       -info.temperature_millicelsius : info.temperature_millicelsius) / 1000u);
        console_put('.');
        console_number((uint64_t)(info.temperature_millicelsius < 0 ?
                       -info.temperature_millicelsius : info.temperature_millicelsius) % 1000u / 100u);
        console_write(" C (");
        console_write(info.temperature_source);
        console_put(')');
    } else {
        console_write("not available from this CPU/platform");
    }
    console_write("\n| Cache        : L1D ");
    console_number(info.l1_data_kib);
    console_write(" KiB, L1I ");
    console_number(info.l1_instruction_kib);
    console_write(" KiB, L2 ");
    console_number(info.l2_kib);
    console_write(" KiB, L3 ");
    console_number(info.l3_kib);
    console_write(" KiB");
    console_write(localized("\n| CPU features : ", "\n| CPU-Funktionen: ",
                            "\n| Funciones CPU: ", "\n| Fonctions CPU: "));
    if ((edx & (1u << 25u)) != 0u) {
        console_write("SSE ");
    }
    if ((edx & (1u << 26u)) != 0u) {
        console_write("SSE2 ");
    }
    if ((ecx & (1u << 28u)) != 0u) {
        console_write("AVX ");
    }
    if (capabilities.avx2 != 0u) console_write("AVX2 ");
    if (capabilities.avx512f != 0u) console_write("AVX-512F ");
    console_write("\n| Core status  : state, not CPU usage\n|\n");
    if (logical_cpus > 16u) {
        logical_cpus = 16u;
    }
    for (core = 0; core < logical_cpus; core++) {
        console_write("| Core ");
        console_number(core);
        console_write(core == 0u ? " : RUNNING     boot processor\n"
                                 : " : DETECTED    not started\n");
    }
    console_colored("+-------------------------------------------------+\n", 0x0Eu);
}

static void show_about(void)
{
    console_colored("+==================== MVHCLOUD ====================+\n", 0x0Bu);
    console_write(localized("| Product      : MVH Kernel\n", "| Produkt      : MVH Kernel\n",
                            "| Producto     : MVH Kernel\n", "| Produit      : MVH Kernel\n"));
    console_write("| Release      : " MVH_KERNEL_VERSION "\n");
    console_write(localized("| Architecture : x86_64 / ELF64\n", "| Architektur  : x86_64 / ELF64\n",
                            "| Arquitectura : x86_64 / ELF64\n", "| Architecture : x86_64 / ELF64\n"));
    console_write("| Website      : https://MVHCLOUD.com\n");
    console_write(localized("| Language     : English\n", "| Sprache      : Deutsch\n",
                            "| Idioma       : Espanol\n", "| Langue       : Francais\n"));
    console_write("| Keyboard     : English US QWERTY\n");
    console_write(localized("| Drivers      : VGA, PS/2, UART, RTC, PCI, PIT\n",
                            "| Treiber      : VGA, PS/2, UART, RTC, PCI, PIT\n",
                            "| Controladores: VGA, PS/2, UART, RTC, PCI, PIT\n",
                            "| Pilotes      : VGA, PS/2, UART, RTC, PCI, PIT\n"));
    console_write("| License      : MIT\n");
    console_colored("+--------------------------------------------------+\n", 0x0Bu);
    console_write(localized("A standalone kernel foundation for low-level experiments.\nNo bootloader or OS distribution is included.\n",
                            "Ein eigenstaendiger Kernel fuer Low-Level-Experimente.\nBootloader und OS-Distribution sind nicht enthalten.\n",
                            "Un kernel independiente para experimentos de bajo nivel.\nNo incluye cargador ni distribucion de sistema.\n",
                            "Un noyau autonome pour les experimentations bas niveau.\nAucun chargeur ni distribution systeme n'est inclus.\n"));
}

static void command_bootinfo(void)
{
    const mvh_bootinfo_snapshot_t *info = bootinfo_current();
    console_colored("Boot handoff\n", 0x0Eu);
    console_write("  Contract       : ");
    if (info->versioned != 0u) {
        console_write("BootInfo V");
        console_number(info->version);
        console_write("\n  Structure size : ");
        console_number(info->size);
        console_write(" bytes\n  Flags          : ");
        console_hex64(info->flags);
    } else console_write("legacy memory-size argument");
    console_write("\n  Memory         : ");
    console_number(info->memory_kib);
    console_write(" KiB\n  Memory map     : ");
    if ((info->flags & MVH_BOOTINFO_FLAG_MEMORY_MAP) != 0u) {
        console_number(info->memory_map_entries);
        console_write(" entries at ");
        console_hex64(info->memory_map_address);
    } else console_write("not supplied");
    console_write("\n  ACPI RSDP      : ");
    if ((info->flags & MVH_BOOTINFO_FLAG_ACPI_RSDP) != 0u) console_hex64(info->acpi_rsdp_address);
    else console_write("not supplied");
    console_write("\n  SMBIOS         : ");
    if ((info->flags & MVH_BOOTINFO_FLAG_SMBIOS) != 0u) console_hex64(info->smbios_address);
    else console_write("not supplied");
    console_write("\n  Framebuffer    : ");
    if ((info->flags & MVH_BOOTINFO_FLAG_FRAMEBUFFER) != 0u) {
        console_number(info->framebuffer.width);
        console_write("x");
        console_number(info->framebuffer.height);
        console_write("x");
        console_number(info->framebuffer.bits_per_pixel);
    } else console_write("not supplied");
    console_write("\n  Random seed    : ");
    if ((info->flags & MVH_BOOTINFO_FLAG_RANDOM_SEED) != 0u) {
        console_number(info->random_seed_size);
        console_write(" bytes");
    } else console_write("not supplied");
    console_write("\n  Command line   : ");
    if ((info->flags & MVH_BOOTINFO_FLAG_COMMAND_LINE) != 0u) {
        console_number(info->command_line_size);
        console_write(" bytes");
    } else console_write("not supplied");
    console_write("\n");
}

static void command_acpiinfo(void)
{
    const acpi_status_t *status = acpi_status();
    console_colored("ACPI firmware information\n", 0x0Eu);
    if (status->available == 0u) {
        console_write("  ACPI           : unavailable or rejected\n");
        return;
    }
    console_write("  Revision       : ");
    console_number(status->revision);
    console_write("\n  Root table     : ");
    console_write(status->uses_xsdt != 0u ? "XSDT" : "RSDT");
    console_write("\n  Tables         : ");
    console_number(status->table_count);
    console_write("\n  Rejected       : ");
    console_number(status->rejected_tables);
    console_write("\n  Unknown        : ");
    console_number(status->unknown_tables);
    console_write("\n  MCFG segments  : ");
    console_number(status->mcfg_segments);
    console_write("\n  SRAT CPU/memory: ");
    console_number(status->srat_cpu_affinities);
    console_write("/");
    console_number(status->srat_memory_affinities);
    console_write("\n  SLIT localities: ");
    console_number(status->slit_localities);
    console_write("\n  PM timer port  : ");
    console_hex32(status->pm_timer_port);
    console_write("\n");
}

static void command_acpitables(void)
{
    acpi_table_info_t table;
    uint32_t index;
    if (acpi_status()->available == 0u) {
        console_write("ACPI is unavailable\n");
        return;
    }
    console_colored("ACPI table registry\n", 0x0Eu);
    for (index = 0u; index < acpi_table_count(); index++) {
        if (acpi_table_info(index, &table) != 0) continue;
        console_write("  ");
        console_write(table.signature);
        console_write("  address=");
        console_hex64(table.address);
        console_write(" length=");
        console_number(table.length);
        console_write(" revision=");
        console_number(table.revision);
        console_write("\n");
    }
}

static void command_madtinfo(void)
{
    const acpi_status_t *status = acpi_status();
    if (acpi_find_table("APIC", 0u) == 0) {
        console_write("MADT is unavailable\n");
        return;
    }
    console_colored("MADT interrupt topology\n", 0x0Eu);
    console_write("  LAPIC address  : ");
    console_hex64(status->lapic_address);
    console_write("\n  LAPIC/x2APIC   : ");
    console_number(status->local_apics);
    console_write("/");
    console_number(status->local_x2apics);
    console_write("\n  IOAPICs        : ");
    console_number(status->ioapics);
    console_write("\n  IRQ overrides  : ");
    console_number(status->interrupt_overrides);
    console_write("\n  NMI entries    : ");
    console_number(status->nmi_sources + status->local_apic_nmis);
    console_write("\n");
}

static void command_ioapicinfo(void)
{
    const apic_status_t *runtime = apic_status();
    acpi_ioapic_info_t ioapic;
    acpi_interrupt_override_t route;
    uint32_t index;
    console_colored("IOAPIC firmware routing\n", 0x0Eu);
    if (acpi_ioapic_count() == 0u) {
        console_write("  IOAPICs        : unavailable\n");
        return;
    }
    for (index = 0u; index < acpi_ioapic_count(); index++) {
        if (acpi_ioapic_info(index, &ioapic) != 0) continue;
        console_write("  IOAPIC ");
        console_number(ioapic.id);
        console_write(" address=");
        console_hex32(ioapic.address);
        console_write(" GSI-base=");
        console_number(ioapic.global_interrupt_base);
        console_write("\n");
    }
    for (index = 0u; index < acpi_interrupt_override_count(); index++) {
        if (acpi_interrupt_override_info(index, &route) != 0) continue;
        console_write("  IRQ ");
        console_number(route.source_irq);
        console_write(" -> GSI ");
        console_number(route.global_interrupt);
        console_write(" flags=");
        console_hex16(route.flags);
        console_write("\n");
    }
    console_write("  Runtime state  : ");
    if (runtime->ioapic_enabled != 0u) {
        console_write("active, timer GSI ");
        console_number(runtime->timer_gsi);
        console_write(", redirections ");
        console_number(runtime->ioapic_redirections);
        console_write("\n  Local APIC    : ID ");
        console_number(runtime->local_apic_id);
        console_write(" version ");
        console_number(runtime->local_apic_version);
        console_write(" at ");
        console_hex64(runtime->local_apic_address);
        console_write("\n  Legacy PIC    : disabled\n");
    } else console_write("legacy PIC fallback active\n");
}

static void command_mcfginfo(void)
{
    const pci_status_t *runtime = pci_status();
    acpi_mcfg_segment_t segment;
    uint32_t index;
    console_colored("PCIe MCFG segments\n", 0x0Eu);
    if (acpi_mcfg_segment_count() == 0u) {
        console_write("  MCFG segments  : unavailable\n");
        return;
    }
    for (index = 0u; index < acpi_mcfg_segment_count(); index++) {
        if (acpi_mcfg_segment_info(index, &segment) != 0) continue;
        console_write("  Segment ");
        console_number(segment.segment_group);
        console_write(" base=");
        console_hex64(segment.base_address);
        console_write(" buses=");
        console_number(segment.start_bus);
        console_write("-");
        console_number(segment.end_bus);
        console_write("\n");
    }
    console_write("  Runtime state  : ");
    console_write(runtime->ecam_enabled != 0u ? "ECAM active through remapped MMIO window\n" :
                                               "legacy CF8/CFC fallback\n");
    console_write("  Config reads   : ");
    console_number(runtime->config_reads);
    console_write("\n  Config writes  : ");
    console_number(runtime->config_writes);
    console_write("\n");
}

static void command_smpinfo(void)
{
    acpi_cpu_info_t cpu;
    uint32_t index;
    console_colored("Firmware CPU topology\n", 0x0Eu);
    console_write("  CPUs described : ");
    console_number(acpi_cpu_count());
    console_write("\n");
    for (index = 0u; index < acpi_cpu_count(); index++) {
        if (acpi_cpu_info(index, &cpu) != 0) continue;
        console_write("  CPU uid=");
        console_number(cpu.processor_uid);
        console_write(" apic-id=");
        console_number(cpu.apic_id);
        console_write(cpu.x2apic != 0u ? " x2APIC" : " xAPIC");
        console_write(cpu.enabled != 0u ? " enabled" :
                      (cpu.online_capable != 0u ? " online-capable" : " disabled"));
        console_write(index == 0u ? " BSP-running\n" : " AP-not-started\n");
    }
}

static void command_hpetinfo(void)
{
    const acpi_status_t *status = acpi_status();
    const hpet_status_t *runtime = hpet_status();
    if (acpi_find_table("HPET", 0u) == 0) {
        console_write("HPET table is unavailable\n");
        return;
    }
    console_colored("HPET firmware table\n", 0x0Eu);
    console_write("  MMIO address   : ");
    console_hex64(status->hpet_address);
    console_write("\n  Runtime state  : ");
    if (runtime->enabled != 0u) {
        console_write("active\n  Frequency      : ");
        console_number(runtime->frequency_hz);
        console_write(" Hz\n  Counter        : ");
        console_number(hpet_ticks());
        console_write("\n  Monotonic ns   : ");
        console_number(hpet_nanoseconds());
        console_write("\n  Timers         : ");
        console_number(runtime->timer_count);
        console_write(runtime->counter_64bit != 0u ? ", 64-bit counter\n" : ", 32-bit counter\n");
    } else console_write("unavailable, PIT fallback\n");
}

static void console_optional_text(const char *text)
{
    console_write(text[0] != '\0' ? text : "not supplied");
}

static void command_smbiosinfo(void)
{
    const smbios_info_t *info = smbios_info();
    console_colored("SMBIOS platform information\n", 0x0Eu);
    if (info->available == 0u) {
        console_write("  SMBIOS         : unavailable or rejected\n");
        return;
    }
    console_write("  Entry point    : ");
    console_write(info->entry_point_64 != 0u ? "SMBIOS 3.x (64-bit)" : "SMBIOS 2.x");
    console_write("\n  Version        : ");
    console_number(info->major);
    console_write(".");
    console_number(info->minor);
    console_write("\n  Table address  : ");
    console_hex64(info->table_address);
    console_write("\n  Structures     : ");
    console_number(info->parsed_structures);
    console_write("\n  BIOS           : ");
    console_optional_text(info->bios_vendor);
    console_write(" / ");
    console_optional_text(info->bios_version);
    console_write("\n  System         : ");
    console_optional_text(info->system_manufacturer);
    console_write(" / ");
    console_optional_text(info->system_product);
    console_write("\n  Board          : ");
    console_optional_text(info->baseboard_manufacturer);
    console_write(" / ");
    console_optional_text(info->baseboard_product);
    console_write("\n  CPU sockets    : ");
    console_number(info->processor_structures);
    console_write("\n  Memory devices : ");
    console_number(info->populated_memory_devices);
    console_write("/");
    console_number(info->memory_device_structures);
    console_write("\n  Installed RAM  : ");
    console_number(info->installed_memory_mib);
    console_write(" MiB\n  Maximum speed  : ");
    console_number(info->maximum_memory_speed_mhz);
    console_write(" MHz\n");
}

static void command_pmmstat(void)
{
    pmm_stats_t stats;
    pmm_get_stats(&stats);
    console_colored("Physical memory allocator statistics\n", 0x0Eu);
    console_write("  Total/reserved : ");
    console_number(stats.total_pages);
    console_write("/");
    console_number(stats.reserved_pages);
    console_write(" pages\n  Used/free      : ");
    console_number(stats.used_pages);
    console_write("/");
    console_number(stats.free_pages);
    console_write(" pages\n  Alloc/free req : ");
    console_number(stats.allocation_requests);
    console_write("/");
    console_number(stats.free_requests);
    console_write("\n  Failed allocs  : ");
    console_number(stats.failed_allocations);
    console_write("\n  Peak pages     : ");
    console_number(stats.peak_used_pages);
    console_write("\n");
}

static void command_timerstat(void)
{
    cpu_info_t info;
    cpu_capabilities_t capabilities;
    cpu_get_info(&info);
    cpu_get_capabilities(&capabilities);
    console_colored("Kernel time sources\n", 0x0Eu);
    console_write("  Clockevent     : PIT legacy periodic\n  Frequency      : ");
    console_number(hal_timer_frequency());
    console_write(" Hz\n  Ticks          : ");
    console_number(hal_ticks());
    console_write("\n  Uptime         : ");
    console_number(hal_uptime_seconds());
    console_write(" seconds\n  TSC            : ");
    console_write(capabilities.tsc != 0u ? "available" : "unavailable");
    console_write("\n  Invariant TSC  : ");
    console_write(capabilities.invariant_tsc != 0u ? "yes" : "no");
    console_write("\n  TSC frequency  : ");
    console_number(info.tsc_hz);
    console_write(" Hz\n  HPET metadata  : ");
    console_write(acpi_status()->hpet_address != 0u ? "available, inactive" : "unavailable");
    console_write("\n");
}

static void command_randomstat(void)
{
    cpu_capabilities_t capabilities;
    cpu_get_capabilities(&capabilities);
    console_colored("Kernel entropy status\n", 0x0Eu);
    console_write("  Ready          : ");
    console_write(random_is_ready() != 0u ? "yes" : "no");
    console_write("\n  Estimated bits : ");
    console_number(random_entropy_bits());
    console_write("/256\n  Boot seed      : ");
    console_write((bootinfo_current()->flags & MVH_BOOTINFO_FLAG_RANDOM_SEED) != 0u ?
                  "mixed" : "not supplied");
    console_write("\n  RDRAND/RDSEED  : ");
    console_write(capabilities.rdrand != 0u ? "yes" : "no");
    console_write("/");
    console_write(capabilities.rdseed != 0u ? "yes" : "no");
    console_write("\n");
}

static void command_securityinfo(void)
{
    cpu_capabilities_t capabilities;
    cpu_security_state_t enabled;
    cpu_get_capabilities(&capabilities);
    cpu_get_security_state(&enabled);
    console_colored("Kernel security policy\n", 0x0Eu);
    console_write("  Kernel sections: RX / RO-NX / RW-NX\n  Null page      : unmapped\n  Write protect  : ");
    console_write(enabled.write_protect != 0u ? "enabled" : "disabled");
    console_write("\n  NX available/on: ");
    console_write(capabilities.nx != 0u ? "yes" : "no");
    console_write("/");
    console_write(enabled.nx != 0u ? "yes" : "no");
    console_write("\n  SMEP enabled   : ");
    console_write(enabled.smep != 0u ? "yes" : "no");
    console_write("\n  SMAP enabled   : ");
    console_write(enabled.smap != 0u ? "yes" : "no");
    console_write("\n  UMIP enabled   : ");
    console_write(enabled.umip != 0u ? "yes" : "no");
    console_write("\n  Stack protector: strong/global guard\n  KASLR          : not implemented\n");
}

static void command_firmwareinfo(void)
{
    console_colored("Firmware handoff summary\n", 0x0Eu);
    console_write("  Boot contract  : ");
    console_write(bootinfo_current()->versioned != 0u ? "BootInfo V2" : "legacy");
    console_write("\n  ACPI           : ");
    console_write(acpi_status()->available != 0u ? "validated" : "unavailable");
    console_write("\n  SMBIOS         : ");
    console_write(smbios_info()->available != 0u ? "validated" : "unavailable");
    console_write("\n  Framebuffer    : ");
    console_write((bootinfo_current()->flags & MVH_BOOTINFO_FLAG_FRAMEBUFFER) != 0u ?
                  "metadata supplied" : "not supplied");
    console_write("\n");
}

static void show_statistics(void)
{
    char input;
    vga_cursor_disable();
    vga_clear();
    console_colored("+==================================================+\n", 0x0Bu);
    console_colored("|            MVH KERNEL STATISTICS                 |\n", 0x0Fu);
    console_colored("+==================================================+\n\n", 0x0Bu);
    show_ram();
    console_write("\n");
    show_cpu();
    console_colored(localized("\nPress Q or Ctrl+C to return to the shell.\n",
                              "\nQ oder Strg+C druecken, um zur Shell zurueckzukehren.\n",
                              "\nPulsa Q o Ctrl+C para volver a la consola.\n",
                              "\nAppuyez sur Q ou Ctrl+C pour revenir au terminal.\n"), 0x0Au);
    for (;;) {
        input = hal_keyboard_read();
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
    reboot_step(localized("Disabling CPU interrupts  ", "CPU-Interrupts aus       ",
                          "Desactivando interrupciones", "Arret des interruptions  "));
    reboot_step(localized("Preparing reset controller", "Reset-Controller bereit  ",
                          "Preparando controlador    ", "Preparation du controleur"));
    console_colored(localized("Sending hardware reset", "Hardware-Reset senden",
                              "Enviando reinicio", "Envoi du redemarrage"), 0x0Fu);
    console_colored(".....\n", 0x0Eu);
    hal_reboot();
}

static void command_ls(const char *path)
{
    fs_entry_t entries[FS_LIST_MAX];
    int count = vfs_list(path, entries, FS_LIST_MAX);
    int index;
    if (count < 0) {
        console_write("ls: path not found\n");
        return;
    }
    for (index = 0; index < count; index++) {
        if (entries[index].is_directory != 0u) {
            console_colored("[DIR]  ", 0x0Bu);
            console_write(entries[index].name);
        } else {
            console_write("[FILE] ");
            console_write(entries[index].name);
            console_write("  ");
            console_number(entries[index].size);
            console_write(" bytes");
        }
        console_write("\n");
    }
    if (count == 0) {
        console_write("Directory is empty.\n");
    }
}

static void command_cat(const char *path)
{
    const char *data;
    uint16_t size;
    uint16_t index;
    if (path[0] == '\0' || vfs_read(path, &data, &size) != 0) {
        console_write("cat: file not found\n");
        return;
    }
    for (index = 0; index < size; index++) {
        console_put(data[index]);
    }
    if (size == 0u || data[size - 1u] != '\n') {
        console_write("\n");
    }
}

static void command_date(void)
{
    rtc_time_t time;
    hal_clock_read(&time);
    console_number(time.year);
    console_put('-');
    console_two_digits(time.month);
    console_put('-');
    console_two_digits(time.day);
    console_put(' ');
    console_two_digits(time.hour);
    console_put(':');
    console_two_digits(time.minute);
    console_put(':');
    console_two_digits(time.second);
    console_write(" UTC\n");
}

static void command_lspci(void)
{
    pci_device_t devices[32];
    uint32_t count = hal_pci_scan(devices, 32u);
    uint32_t index;
    uint32_t bar;
    for (index = 0; index < count; index++) {
        console_number(devices[index].bus);
        console_put(':');
        console_number(devices[index].slot);
        console_put('.');
        console_number(devices[index].function);
        console_write("  ");
        console_hex16(devices[index].vendor);
        console_put(':');
        console_hex16(devices[index].device);
        console_write("  ");
        console_write(pci_class_name(devices[index].class_code));
        console_write("  IRQ ");
        if (devices[index].irq_pin == 0u) console_write("none");
        else console_number(devices[index].irq_line);
        console_write("\n");
        for (bar = 0u; bar < devices[index].bar_count; bar++) {
            if (devices[index].bar_base[bar] == 0u) continue;
            console_write("    BAR"); console_number(bar); console_write(": ");
            console_write(devices[index].bar_is_io[bar] != 0u ? "I/O " : "MMIO ");
            if (devices[index].bar_is_64[bar] != 0u) console_write("64-bit ");
            console_hex64(devices[index].bar_base[bar]);
            console_write("\n");
        }
    }
    if (count == 0u) {
        console_write("No PCI devices found.\n");
    }
}

static void command_blockdev(void)
{
    block_device_t devices[BLOCK_DEVICE_MAX];
    uint32_t count = block_list(devices, BLOCK_DEVICE_MAX);
    uint32_t index;
    if (count == 0u) {
        console_write("No block devices registered.\n");
        return;
    }
    console_write("ID  NAME  SECTORS  BYTES/SECTOR  MODE  TABLE\n");
    for (index = 0u; index < count; index++) {
        partition_info_t partitions;
        console_number(devices[index].id);
        console_write("  ");
        console_write(devices[index].name);
        console_write("  ");
        console_number(devices[index].sector_count);
        console_write("  ");
        console_number(devices[index].sector_size);
        console_write(devices[index].writable != 0u ? "  rw  " : "  ro  ");
        if (partition_probe(devices[index].id, &partitions) == 0) {
            console_write(partition_table_name(partitions.type));
            console_write(" (");
            console_number(partitions.partition_count);
            console_write(")");
            if (partitions.type == PARTITION_TABLE_GPT) {
                console_write(" verified usable=");
                console_number(partitions.first_usable_lba);
                console_write("-");
                console_number(partitions.last_usable_lba);
            }
        } else {
            console_write("unavailable");
        }
        console_write("\n");
    }
}

static int persistent_ready(void)
{
    if (persistent_filesystem.mounted != 0u) return 1;
    console_write("MVHFS is not mounted; use pmount <device-id> or pmkfs <device-id>\n");
    return 0;
}

static void command_pmkfs(const char *argument)
{
    uint64_t device_id;
    if (!parse_number(argument, &device_id) || device_id >= BLOCK_DEVICE_MAX) {
        console_write("Usage: pmkfs <device-id>\n");
        return;
    }
    if (mvhfs_format((uint32_t)device_id) != 0 ||
        mvhfs_mount(&persistent_filesystem, (uint32_t)device_id) != 0) {
        console_write("pmkfs: writable 512-byte block device with at least 522 sectors required\n");
        return;
    }
    console_write("MVHFS formatted and mounted\n");
}

static void command_pmount(const char *argument)
{
    uint64_t device_id;
    if (!parse_number(argument, &device_id) || device_id >= BLOCK_DEVICE_MAX ||
        mvhfs_mount(&persistent_filesystem, (uint32_t)device_id) != 0) {
        console_write("pmount: valid MVHFS device required\n");
        return;
    }
    console_write("MVHFS mounted, generation ");
    console_number(persistent_filesystem.generation);
    console_write("\n");
}

static void command_pls(void)
{
    uint32_t count;
    uint32_t index;
    if (!persistent_ready()) return;
    count = mvhfs_list(&persistent_filesystem, persistent_entries, MVHFS_MAX_FILES);
    for (index = 0u; index < count && index < MVHFS_MAX_FILES; index++) {
        console_write(persistent_entries[index].name);
        console_write("  ");
        console_number(persistent_entries[index].size);
        console_write(" bytes\n");
    }
    if (count == 0u) console_write("MVHFS is empty\n");
}

static void command_pwrite(const char *argument)
{
    char name[MVHFS_NAME_MAX];
    const char *text;
    uint32_t size = 0u;
    if (!persistent_ready()) return;
    if (split_path_text(argument, name, MVHFS_NAME_MAX, &text) != 0) {
        console_write("Usage: pwrite <name> <text>\n");
        return;
    }
    while (size < MVHFS_FILE_MAX && text[size] != '\0') size++;
    if (size == MVHFS_FILE_MAX) {
        console_write("pwrite: file exceeds 4096 bytes\n");
        return;
    }
    mvhfs_create(&persistent_filesystem, name);
    if (mvhfs_write(&persistent_filesystem, name, text, size) != 0)
        console_write("pwrite: write failed\n");
}

static void command_pcat(const char *name)
{
    uint32_t size;
    uint32_t index;
    int result;
    if (!persistent_ready()) return;
    result = mvhfs_read(&persistent_filesystem, name, persistent_file_buffer,
                        sizeof(persistent_file_buffer), &size);
    if (result != 0) {
        console_write(result == -3 ? "pcat: checksum mismatch\n" : "pcat: file not found\n");
        return;
    }
    for (index = 0u; index < size; index++) console_put((char)persistent_file_buffer[index]);
    console_write("\n");
}

static void command_prm(const char *name)
{
    if (!persistent_ready()) return;
    if (mvhfs_remove(&persistent_filesystem, name) != 0)
        console_write("prm: file not found\n");
}

static void command_drivers(void)
{
    console_colored("Loaded kernel drivers\n", 0x0Bu);
    console_write("  vga-text-cursor       display and hardware cursor\n");
    console_write("  ps2-keyboard-en-us    keyboard, Shift, Caps, Ctrl\n");
    console_write("  serial-uart-16550     COM1 debug output\n");
    console_write("  x86-cpuid             CPU detection\n");
    console_write("  cmos-rtc              real-time clock\n");
    console_write("  pci-config            PCI device discovery\n");
    console_write("  x86-idt-pic           interrupt controller\n");
    console_write("  x86-exceptions        CPU exception handling\n");
    console_write("  pit-8254              system timer\n");
    console_write("  ramfs                 volatile filesystem\n");
    console_write("  mvhfs                 persistent block filesystem\n");
    console_write("  device-manager        kernel device registry\n");
}

static void command_utilinfo(void)
{
    console_colored("Kernel utility API\n", 0x0Eu);
    console_write("  Memory APIs    : 5\n");
    console_write("  String APIs    : 13\n");
    console_write("  ASCII APIs     : 12\n");
    console_write("  Integer/bit APIs: 20\n");
    console_write("  Total          : 50 (native-test covered)\n");
}

static void feature_line(const char *name, uint32_t available, const char *description)
{
    console_write("  ");
    console_write(available != 0u ? "[yes] " : "[ no] ");
    console_write(name);
    console_write(" - ");
    console_write(description);
    console_write("\n");
}

static void command_features(void)
{
    uint32_t ecx = cpu_feature_ecx();
    uint32_t edx = cpu_feature_edx();
    uint32_t extended_edx = cpu_extended_feature_edx();
    cpu_capabilities_t capabilities;
    cpu_security_state_t security;
    cpu_get_capabilities(&capabilities);
    cpu_get_security_state(&security);
    console_colored("CPU hardware features\n", 0x0Eu);
    feature_line("FPU", edx & (1u << 0u), "floating-point calculations");
    feature_line("TSC", edx & (1u << 4u), "CPU timestamp counter");
    feature_line("MSR", edx & (1u << 5u), "model-specific registers");
    feature_line("APIC", edx & (1u << 9u), "advanced interrupt controller");
    feature_line("MMX", edx & (1u << 23u), "packed integer instructions");
    feature_line("SSE", edx & (1u << 25u), "streaming SIMD instructions");
    feature_line("SSE2", edx & (1u << 26u), "extended SIMD instructions");
    feature_line("SSE3", ecx & (1u << 0u), "third-generation SIMD");
    feature_line("SSSE3", ecx & (1u << 9u), "supplemental SIMD instructions");
    feature_line("SSE4.1", ecx & (1u << 19u), "fourth-generation SIMD");
    feature_line("SSE4.2", ecx & (1u << 20u), "CRC and string instructions");
    feature_line("AES", ecx & (1u << 25u), "AES acceleration");
    feature_line("AVX", ecx & (1u << 28u), "256-bit vector hardware");
    feature_line("AVX2", capabilities.avx2, "second-generation 256-bit vectors");
    feature_line("AVX-512F", capabilities.avx512f, "512-bit vector foundation");
    feature_line("XSAVE", capabilities.xsave, "extended CPU state save/restore");
    feature_line("RDRAND", ecx & (1u << 30u), "hardware random numbers");
    feature_line("RDSEED", capabilities.rdseed, "hardware entropy seed");
    feature_line("x2APIC", capabilities.x2apic, "extended local APIC mode");
    feature_line("PAT", capabilities.pat, "page attribute table");
    feature_line("MTRR", capabilities.mtrr, "memory type range registers");
    feature_line("NX", extended_edx & (1u << 20u), "non-executable memory pages");
    feature_line("LONG MODE", extended_edx & (1u << 29u), "64-bit execution mode");
    console_write("\nKernel protection: WP ");
    console_write(security.write_protect != 0u ? "on" : "off");
    console_write(", NX "); console_write(security.nx != 0u ? "on" : "off");
    console_write(", SMEP "); console_write(security.smep != 0u ? "on" : "off");
    console_write(", SMAP "); console_write(security.smap != 0u ? "on" : "off");
    console_write(", UMIP "); console_write(security.umip != 0u ? "on\n" : "off\n");
    console_write("Additional detected cores require SMP startup support.\n");
}

static void command_netinfo(void)
{
    console_colored("Network protocol core\n", 0x0Eu);
    console_write("  Ethernet II    : frame parse/build\n");
    console_write("  ARP            : request/reply codec, expiring 32-entry cache\n");
    console_write("  IPv4           : header validation/build, checksum, fragmentation metadata\n");
    console_write("  ICMP           : validated echo-reply generation\n");
    console_write("  UDP            : datagram parse/build with pseudo-header checksum\n");
    console_write("  Routing        : 16 routes, longest-prefix and metric selection\n");
    console_write("  NIC state      : no supported hardware interface attached\n");
}

static void command_random(void)
{
    uint32_t bits = random_entropy_bits();
    console_write("Entropy pool: ");
    console_number(bits);
    console_write("/256 estimated bits (CSPRNG ");
    if (random_is_ready() == 0u) {
        console_write("not ready)\nSample withheld until the pool is ready.\n");
        return;
    }
    console_write("ready)\nRandom sample: ");
    console_hex64(random_u64());
    console_write("\n");
}

static void command_crc32(const char *text)
{
    uint32_t size = 0u;
    if (text[0] == '\0') {
        console_write("Usage: crc32 <text>\n");
        return;
    }
    while (text[size] != '\0') size++;
    console_hex32(crc32(text, size));
    console_write("\n");
}

static void command_uptime(void)
{
    uint64_t seconds = hal_uptime_seconds();
    uint64_t days = seconds / 86400u;
    uint64_t hours;
    uint64_t minutes;
    seconds %= 86400u;
    hours = seconds / 3600u;
    seconds %= 3600u;
    minutes = seconds / 60u;
    seconds %= 60u;
    console_write("up ");
    if (days != 0u) {
        console_number(days);
        console_write(days == 1u ? " day, " : " days, ");
    }
    console_number(hours);
    console_write("h ");
    console_number(minutes);
    console_write("m ");
    console_number(seconds);
    console_write("s\n");
}

static void command_meminfo(void)
{
    pmm_stats_t stats;
    pmm_get_stats(&stats);
    console_write("MemTotal:     ");
    console_number(stats.total_pages * 4u);
    console_write(" KiB\nMemUsed:      ");
    console_number(stats.used_pages * 4u);
    console_write(" KiB\nMemAvailable: ");
    console_number(stats.free_pages * 4u);
    console_write(" KiB\nReserved:     ");
    console_number(stats.reserved_pages * 4u);
    console_write(" KiB\nHeapTotal:    ");
    console_number(heap_total_bytes() / 1024u);
    console_write(" KiB\nHeapUsed:     ");
    console_number(heap_used_bytes() / 1024u);
    console_write(" KiB\n");
    console_write("PageAlloc:    "); console_number(stats.allocation_requests);
    console_write("\nPageFree:     "); console_number(stats.free_requests);
    console_write("\nPageFailures: "); console_number(stats.failed_allocations);
    console_write("\nPagePeak:     "); console_number(stats.peak_used_pages);
    console_write(" pages\nMappedPages:  "); console_number(vmm_mapped_pages());
    console_write("\n");
}

static void command_devices(void)
{
    device_info_t devices[DEVICE_MAX];
    uint32_t count = device_list(devices, DEVICE_MAX);
    uint32_t index;
    console_write("ID  TYPE        STATE   NAME\n");
    for (index = 0u; index < count; index++) {
        console_number(devices[index].id);
        console_write("   ");
        console_write(device_type_name(devices[index].type));
        console_write("   ");
        console_write(devices[index].online != 0u ? "ONLINE  " : "OFFLINE ");
        console_write(devices[index].name);
        console_write("\n");
    }
}

static void command_free(void)
{
    pmm_stats_t stats;
    pmm_get_stats(&stats);
    console_write("              total       used       free\n");
    console_write("Memory:   ");
    console_number(stats.total_pages * 4u);
    console_write(" KiB  ");
    console_number(stats.used_pages * 4u);
    console_write(" KiB  ");
    console_number(stats.free_pages * 4u);
    console_write(" KiB\nHeap:     ");
    console_number(heap_total_bytes() / 1024u);
    console_write(" KiB  ");
    console_number(heap_used_bytes() / 1024u);
    console_write(" KiB  allocations: ");
    console_number(heap_allocation_count());
    console_write("\n");
}

static void command_heapinfo(void)
{
    heap_stats_t stats;
    heap_get_stats(&stats);
    console_write("Heap total:          "); console_number(stats.total_bytes); console_write(" bytes\n");
    console_write("Heap used:           "); console_number(stats.used_bytes); console_write(" bytes\n");
    console_write("Heap free payload:   "); console_number(stats.free_bytes); console_write(" bytes\n");
    console_write("Largest free block:  "); console_number(stats.largest_free_block); console_write(" bytes\n");
    console_write("Blocks/free blocks:  "); console_number(stats.blocks); console_write("/");
    console_number(stats.free_blocks); console_write("\nAllocations active:  ");
    console_number(stats.allocations); console_write("\nAllocation failures: ");
    console_number(stats.allocation_failures); console_write("\nInvalid frees:       ");
    console_number(stats.invalid_frees); console_write("\nIntegrity:           ");
    console_write(heap_validate() == 0 ? "valid\n" : "CORRUPTED\n");
}

static void command_cpuinfo(void)
{
    uint64_t random;
    cpu_info_t info;
    cpu_capabilities_t capabilities;
    cpu_get_info(&info);
    cpu_get_capabilities(&capabilities);
    show_cpu();
    console_write("TSC:          "); console_hex64(cpu_read_tsc());
    console_write("\nTSC estimate: "); console_number(info.tsc_hz); console_write(" Hz\n");
    console_write("XSAVE area:   "); console_number(info.xsave_bytes); console_write(" bytes\n");
    console_write("Microcode:    ");
    if (info.microcode_available != 0u) console_hex64(info.microcode);
    else console_write("not exposed safely by this platform");
    console_write("\nHardware RNG: ");
    if (capabilities.rdrand != 0u && cpu_random64(&random) != 0u) console_hex64(random);
    else console_write("not available");
    console_write("\n");
}

static void command_irqstat(void)
{
    uint64_t total;
    uint64_t timer;
    uint64_t spurious;
    interrupt_disable();
    total = interrupt_total();
    timer = interrupt_count(32u);
    spurious = interrupt_spurious_count();
    interrupt_enable();
    console_write("Interrupt total:    "); console_number(total);
    console_write("\nTimer IRQ0/vector32: "); console_number(timer);
    console_write("\nSpurious IRQs:       "); console_number(spurious);
    console_write("\n");
}

static void command_pagetable(void)
{
    uintptr_t physical;
    uint64_t flags;
    uintptr_t addresses[4] = {0u, 0x1000u, (uintptr_t)&__kernel_end,
                              ((uintptr_t)&__kernel_end + 0xFFFu) & ~0xFFFull};
    uint32_t index;
    for (index = 0u; index < 4u; index++) {
        console_write("VA "); console_hex64(addresses[index]); console_write(": ");
        if (vmm_query_page(addresses[index], &physical, &flags) != 0) {
            console_write("unmapped\n");
        } else {
            console_write("PA "); console_hex64(physical); console_write(" flags ");
            console_hex64(flags); console_write("\n");
        }
    }
}

static void command_paniccodes(void)
{
    console_write("MVH panic code format\n");
    console_write("  MVH-KERNEL-0001  explicit kernel panic\n");
    console_write("  MVH-EX-00        divide error\n");
    console_write("  MVH-EX-06        invalid opcode\n");
    console_write("  MVH-EX-08        double fault\n");
    console_write("  MVH-EX-0D        general protection fault\n");
    console_write("  MVH-EX-0E        page fault\n");
    console_write("  MVH-EX-12        machine check\n");
    console_write("  MVH-EX-15        control protection\n");
    console_write("Crash output includes vector, decoded error flags, CR0-CR4, registers and stack trace.\n");
}

static void command_ps(void)
{
    task_info_t tasks[TASK_MAX];
    uint32_t count = task_list(tasks, TASK_MAX);
    uint32_t index;
    console_write("PID  PRI  STATE      NAME\n");
    for (index = 0u; index < count; index++) {
        console_number(tasks[index].pid);
        console_write("    ");
        console_number(tasks[index].priority);
        console_write("    ");
        console_write(task_state_name(tasks[index].state));
        console_write("    ");
        console_write(tasks[index].name);
        console_write("\n");
    }
}

static void command_heaptest(void)
{
    console_write(heap_self_test() == 0 ? "Heap self-test passed\n"
                                        : "Heap self-test failed\n");
}

static void command_pagetest(void)
{
    console_write(pmm_self_test() == 0 ? "Page allocator self-test passed\n"
                                       : "Page allocator self-test failed\n");
}

static int selftest_line(const char *name, int result)
{
    console_write(result == 0 ? "[PASS] " : "[FAIL] ");
    console_write(name);
    console_write("\n");
    return result;
}

static void command_selftest(void)
{
    const char *data;
    uint16_t size;
    uint64_t tick_before;
    int failures = 0;
    failures += selftest_line("physical page allocator", pmm_self_test()) != 0;
    failures += selftest_line("kernel heap", heap_self_test()) != 0;
    failures += selftest_line("atomics and locks", sync_self_test()) != 0;
    failures += selftest_line("CRC32 core", crc32_self_test()) != 0;
    failures += selftest_line("BootInfo V2 validator", bootinfo_self_test()) != 0;
    failures += selftest_line("ACPI parser", acpi_self_test()) != 0;
    failures += selftest_line("SMBIOS parser", smbios_self_test()) != 0;
    failures += selftest_line("entropy generator", random_self_test()) != 0;
    failures += selftest_line("block and partition layer", block_self_test()) != 0;
    failures += selftest_line("network protocol core", net_self_test()) != 0;
    failures += selftest_line("heap structure", heap_validate()) != 0;
    failures += selftest_line("null page protection", vmm_query_page(0u, 0, 0) != 0 ? 0 : -1) != 0;
    failures += selftest_line("dynamic page mapping", vmm_self_test()) != 0;
    failures += selftest_line("VFS root", vfs_read("/etc/version", &data, &size) == 0 &&
                              size != 0u ? 0 : -1) != 0;
    failures += selftest_line("device registry", device_count() >= 11u ? 0 : -1) != 0;
    tick_before = hal_ticks();
    hal_sleep_ms(20u);
    failures += selftest_line("timer progress", hal_ticks() > tick_before ? 0 : -1) != 0;
    if (failures == 0) {
        klog_write("INFO", "kernel self-test passed");
        console_write("All kernel self-tests passed\n");
    } else {
        klog_write("ERROR", "kernel self-test failed");
        console_write("Kernel self-test failures: ");
        console_number((uint64_t)failures);
        console_write("\n");
    }
}

static void command_dmesg(void)
{
    char output[4097];
    if (klog_copy(output, sizeof(output)) == 0u) {
        console_write("Kernel log is empty.\n");
        return;
    }
    console_write(output);
}

static void register_platform_devices(void)
{
    device_manager_init();
    device_register("boot-cpu", DEVICE_CPU, 1u);
    device_register("8259-pic", DEVICE_INTERRUPT, 1u);
    device_register("8254-pit", DEVICE_TIMER, 1u);
    device_register("ps2-keyboard", DEVICE_INPUT, 1u);
    device_register("vga-text", DEVICE_DISPLAY, 1u);
    device_register("uart-com1", DEVICE_SERIAL, 1u);
    device_register("cmos-rtc", DEVICE_CLOCK, 1u);
    device_register("pci-config", DEVICE_BUS, 1u);
    device_register("ramfs-root", DEVICE_FILESYSTEM, 1u);
    device_register("entropy-pool", DEVICE_RANDOM, 1u);
    device_register("block-registry", DEVICE_BLOCK, 1u);
}

static void run_command(const char *command)
{
    const char *argument;
    const char *text;
    char path[FS_PATH_MAX];
    char working_directory[FS_PATH_MAX];
    int result;
    if (text_equals(command, "help")) {
        console_colored(localized("Available commands\n", "Verfuegbare Befehle\n",
                                  "Comandos disponibles\n", "Commandes disponibles\n"), 0x0Au);
        console_write(localized(
            "  help         Show this command list\n  about        Show system information\n  statics      Open CPU and RAM monitor\n  language     Show or change language\n  clear        Clear the screen\n  reboot       Restart the system\n",
            "  help         Diese Befehlsliste anzeigen\n  about        Systeminformationen anzeigen\n  statics      CPU- und RAM-Monitor oeffnen\n  language     Sprache anzeigen oder wechseln\n  clear        Bildschirm leeren\n  reboot       System neu starten\n",
            "  help         Mostrar esta lista\n  about        Mostrar informacion del sistema\n  statics      Abrir monitor de CPU y RAM\n  language     Mostrar o cambiar idioma\n  clear        Limpiar la pantalla\n  reboot       Reiniciar el sistema\n",
            "  help         Afficher cette liste\n  about        Afficher les informations systeme\n  statics      Ouvrir le moniteur CPU et RAM\n  language     Afficher ou changer la langue\n  clear        Effacer l'ecran\n  reboot       Redemarrer le systeme\n"));
        console_write("\nFilesystem: ls dir cd pwd mkdir touch write append cat type open rm rmdir mount df\n");
        console_write("Persistent: pmkfs pmount pls pwrite pcat prm\n");
        console_write("System:     date uptime ticks sleep meminfo free devices lspci blockdev drivers features bootinfo netinfo\n");
        console_write("Kernel:     ps dmesg random crc32 selftest heaptest pagetest synctest faulttest\n");
        console_write("Debug:      cpuinfo firmwareinfo acpiinfo acpitables madtinfo ioapicinfo mcfginfo\n");
        console_write("Firmware:   hpetinfo smbiosinfo smpinfo\n");
        console_write("Stats:      pmmstat timerstat randomstat securityinfo utilinfo heapinfo irqstat pagetable paniccodes\n");
        console_write("Info:       uname version hostname whoami\n");
        console_write("Other:      echo clear cls reboot\n");
        console_write("Use '<command> help' is not required; arguments follow the command.\n");
    } else if (text_equals(command, "pwd")) {
        if (vfs_pwd(working_directory, FS_PATH_MAX) == 0) {
            console_write(working_directory);
            console_write("\n");
        }
    } else if ((argument = command_argument(command, "ls")) != 0) {
        command_ls(argument);
    } else if ((argument = command_argument(command, "dir")) != 0) {
        command_ls(argument);
    } else if ((argument = command_argument(command, "cd")) != 0) {
        if (argument[0] == '\0') {
            argument = "/home";
        }
        if (vfs_chdir(argument) != 0) {
            console_write("cd: directory not found\n");
        }
    } else if ((argument = command_argument(command, "mkdir")) != 0) {
        if (argument[0] == '\0' || vfs_mkdir(argument) != 0) {
            console_write("mkdir: cannot create directory\n");
        }
    } else if ((argument = command_argument(command, "touch")) != 0) {
        if (argument[0] == '\0' || vfs_touch(argument) != 0) {
            console_write("touch: cannot create file\n");
        }
    } else if ((argument = command_argument(command, "write")) != 0) {
        if (split_path_text(argument, path, FS_PATH_MAX, &text) != 0) {
            console_write("Usage: write <file> <text>\n");
        } else {
            vfs_touch(path);
            result = vfs_write(path, text, 0u);
            if (result != 0) {
                console_write(result == -2 ? "write: file is full\n" : "write: failed\n");
            }
        }
    } else if ((argument = command_argument(command, "append")) != 0) {
        if (split_path_text(argument, path, FS_PATH_MAX, &text) != 0) {
            console_write("Usage: append <file> <text>\n");
        } else {
            vfs_touch(path);
            result = vfs_write(path, text, 1u);
            if (result != 0) {
                console_write(result == -2 ? "append: file is full\n" : "append: failed\n");
            }
        }
    } else if ((argument = command_argument(command, "cat")) != 0) {
        command_cat(argument);
    } else if ((argument = command_argument(command, "type")) != 0) {
        command_cat(argument);
    } else if ((argument = command_argument(command, "open")) != 0) {
        command_cat(argument);
    } else if ((argument = command_argument(command, "rm")) != 0) {
        result = argument[0] == '\0' ? -1 : vfs_remove(argument);
        if (result != 0) {
            console_write(result == -2 ? "rm: directory is not empty\n" : "rm: path not found\n");
        }
    } else if ((argument = command_argument(command, "rmdir")) != 0) {
        result = argument[0] == '\0' ? -1 : vfs_remove(argument);
        if (result != 0) {
            console_write(result == -2 ? "rmdir: directory is not empty\n" : "rmdir: path not found\n");
        }
    } else if ((argument = command_argument(command, "echo")) != 0) {
        console_write(argument);
        console_write("\n");
    } else if (text_equals(command, "date")) {
        command_date();
    } else if (text_equals(command, "uptime")) {
        command_uptime();
    } else if (text_equals(command, "ticks")) {
        console_number(hal_ticks());
        console_write(" ticks at ");
        console_number(hal_timer_frequency());
        console_write(" Hz\n");
    } else if ((argument = command_argument(command, "sleep")) != 0) {
        uint64_t milliseconds;
        if (!parse_number(argument, &milliseconds) || milliseconds > 60000u) {
            console_write("Usage: sleep <milliseconds>, maximum 60000\n");
        } else {
            hal_sleep_ms(milliseconds);
        }
    } else if (text_equals(command, "meminfo")) {
        command_meminfo();
    } else if (text_equals(command, "free")) {
        command_free();
    } else if (text_equals(command, "heapinfo")) {
        command_heapinfo();
    } else if (text_equals(command, "cpuinfo")) {
        command_cpuinfo();
    } else if (text_equals(command, "bootinfo")) {
        command_bootinfo();
    } else if (text_equals(command, "acpiinfo")) {
        command_acpiinfo();
    } else if (text_equals(command, "acpitables")) {
        command_acpitables();
    } else if (text_equals(command, "madtinfo")) {
        command_madtinfo();
    } else if (text_equals(command, "ioapicinfo")) {
        command_ioapicinfo();
    } else if (text_equals(command, "mcfginfo")) {
        command_mcfginfo();
    } else if (text_equals(command, "smpinfo")) {
        command_smpinfo();
    } else if (text_equals(command, "hpetinfo")) {
        command_hpetinfo();
    } else if (text_equals(command, "smbiosinfo")) {
        command_smbiosinfo();
    } else if (text_equals(command, "firmwareinfo")) {
        command_firmwareinfo();
    } else if (text_equals(command, "pmmstat")) {
        command_pmmstat();
    } else if (text_equals(command, "timerstat")) {
        command_timerstat();
    } else if (text_equals(command, "randomstat")) {
        command_randomstat();
    } else if (text_equals(command, "securityinfo")) {
        command_securityinfo();
    } else if (text_equals(command, "utilinfo")) {
        command_utilinfo();
    } else if (text_equals(command, "irqstat")) {
        command_irqstat();
    } else if (text_equals(command, "pagetable")) {
        command_pagetable();
    } else if (text_equals(command, "paniccodes")) {
        command_paniccodes();
    } else if (text_equals(command, "random")) {
        command_random();
    } else if ((argument = command_argument(command, "crc32")) != 0) {
        command_crc32(argument);
    } else if ((argument = command_argument(command, "pmkfs")) != 0) {
        command_pmkfs(argument);
    } else if ((argument = command_argument(command, "pmount")) != 0) {
        command_pmount(argument);
    } else if (text_equals(command, "pls")) {
        command_pls();
    } else if ((argument = command_argument(command, "pwrite")) != 0) {
        command_pwrite(argument);
    } else if ((argument = command_argument(command, "pcat")) != 0) {
        command_pcat(argument);
    } else if ((argument = command_argument(command, "prm")) != 0) {
        command_prm(argument);
    } else if (text_equals(command, "mount")) {
        console_write("root on / type ");
        console_write(vfs_root_type());
        console_write(" (read-write, volatile)\n");
    } else if (text_equals(command, "df")) {
        console_write("Filesystem  Type   Mounted on\nroot        ");
        console_write(vfs_root_type());
        console_write("  /\n");
    } else if (text_equals(command, "ps")) {
        command_ps();
    } else if (text_equals(command, "heaptest")) {
        command_heaptest();
    } else if (text_equals(command, "pagetest")) {
        command_pagetest();
    } else if (text_equals(command, "synctest")) {
        console_write(sync_self_test() == 0 ? "Synchronization self-test passed\n"
                                            : "Synchronization self-test failed\n");
    } else if (text_equals(command, "selftest")) {
        command_selftest();
    } else if (text_equals(command, "dmesg")) {
        command_dmesg();
    } else if (text_equals(command, "faulttest")) {
        klog_write("WARN", "deliberate breakpoint exception requested");
        console_write("Triggering breakpoint exception.\n");
        __asm__ volatile ("int3");
    } else if (text_equals(command, "faulttest page")) {
        volatile uint64_t *unmapped = (volatile uint64_t *)(uintptr_t)0x40000000u;
        uint64_t value;
        klog_write("WARN", "deliberate page fault requested");
        console_write("Triggering unmapped page access.\n");
        value = *unmapped;
        (void)value;
    } else if (text_equals(command, "devices")) {
        command_devices();
    } else if (text_equals(command, "lspci")) {
        command_lspci();
    } else if (text_equals(command, "blockdev")) {
        command_blockdev();
    } else if (text_equals(command, "drivers")) {
        command_drivers();
    } else if (text_equals(command, "features")) {
        command_features();
    } else if (text_equals(command, "netinfo")) {
        command_netinfo();
    } else if (text_equals(command, "version")) {
        console_write(MVH_KERNEL_NAME " " MVH_KERNEL_VERSION " " MVH_KERNEL_ARCH " " MVH_KERNEL_FORMAT "\n");
    } else if (text_equals(command, "uname") || text_equals(command, "uname -a")) {
        console_write("MVHKernel mvhcloud " MVH_KERNEL_VERSION " " MVH_KERNEL_ARCH "\n");
    } else if (text_equals(command, "hostname")) {
        console_write("mvhcloud\n");
    } else if (text_equals(command, "whoami")) {
        console_write("root\n");
    } else if (text_equals(command, "language")) {
        show_languages();
    } else if (text_equals(command, "language en") || text_equals(command, "language de") ||
               text_equals(command, "language es") || text_equals(command, "language fr")) {
        set_language(command + 9);
        console_write(localized("Language changed to English.\n", "Sprache auf Deutsch geaendert.\n",
                                "Idioma cambiado a Espanol.\n", "Langue changee en Francais.\n"));
    } else if (text_equals(command, "clear") || text_equals(command, "cls")) {
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
    int bootinfo_status = bootinfo_capture(memory_kib, boot_data);
    uint64_t firmware_address;
    uint32_t ata_count;
    klog_init();
    hal_init();
    if (bootinfo_status != 0) {
        kernel_panic("invalid boot handoff data");
    }
    memory_kib = bootinfo_current()->memory_kib;
    cpu_init();
    random_init();
    if ((bootinfo_current()->flags & MVH_BOOTINFO_FLAG_RANDOM_SEED) != 0u) {
        uint32_t estimated_bits = bootinfo_current()->random_seed_size * 4u;
        if (estimated_bits > 128u) estimated_bits = 128u;
        entropy_add((const void *)(uintptr_t)bootinfo_current()->random_seed_address,
                    bootinfo_current()->random_seed_size, estimated_bits);
    }
    block_init();
    net_arp_cache_init();
    net_route_init();
    ata_count = ata_init();
    ASSERT(hal_timer_frequency() != 0u);
    klog_set_console(1u);
    klog_write("INFO", MVH_KERNEL_NAME " " MVH_KERNEL_VERSION " build " MVH_KERNEL_BUILD_ID);
    klog_write("INFO", "kernel ABI " MVH_KERNEL_ABI_STRING "; boot ABI 2 with legacy fallback");
    if (ata_count != 0u)
        klog_write("INFO", "ATA PIO registered writable persistent block storage");
    else klog_write("INFO", "ATA PIO found no compatible disk; block registry remains available");
    if ((bootinfo_current()->flags & MVH_BOOTINFO_FLAG_ACPI_RSDP) != 0u) {
        if (acpi_init(bootinfo_current()->acpi_rsdp_address) == 0)
            klog_write("INFO", "ACPI tables validated and registered");
        else klog_write("WARN", "ACPI handoff was rejected; legacy hardware path remains active");
    } else {
        firmware_address = acpi_discover_rsdp();
        if (firmware_address != 0u && acpi_init(firmware_address) == 0)
            klog_write("INFO", "ACPI RSDP discovered in firmware memory and tables registered");
        else klog_write("INFO", "ACPI unavailable; legacy hardware path remains active");
    }
    if ((bootinfo_current()->flags & MVH_BOOTINFO_FLAG_SMBIOS) != 0u) {
        if (smbios_init(bootinfo_current()->smbios_address) == 0)
            klog_write("INFO", "SMBIOS structures validated and indexed");
        else klog_write("WARN", "SMBIOS handoff was rejected");
    } else {
        firmware_address = smbios_discover_entry();
        if (firmware_address != 0u && smbios_init(firmware_address) == 0)
            klog_write("INFO", "SMBIOS entry discovered in firmware memory and indexed");
        else klog_write("INFO", "SMBIOS unavailable");
    }
    klog_write("INFO", "hardware abstraction layer initialized");
    pmm_init(memory_kib, (uintptr_t)&__kernel_end);
    klog_write("INFO", "physical memory manager initialized");
    if (vmm_init() != 0) {
        kernel_panic("virtual memory manager initialization failed");
    }
    klog_write("INFO", "paging protections and null guard initialized");
    if (framebuffer_init() == 0)
        klog_write("INFO", "linear framebuffer mapped for graphical console");
    else klog_write("INFO", "linear framebuffer unavailable; VGA text console active");
    if (hpet_init() == 0) klog_write("INFO", "HPET monotonic counter activated");
    else klog_write("INFO", "HPET unavailable; PIT remains the monotonic fallback");
    if (pci_init() == 0) klog_write("INFO", "PCIe ECAM configuration access activated");
    else klog_write("INFO", "PCIe ECAM unavailable; legacy PCI configuration remains active");
    if (apic_init() == 0)
        klog_write("INFO", "Local APIC and IOAPIC activated; legacy PIC disabled");
    else klog_write("INFO", "APIC activation unavailable; legacy PIC remains active");
    if (heap_init() != 0) {
        kernel_panic("kernel heap initialization failed");
    }
    klog_write("INFO", "kernel heap initialized");
    vfs_init();
    klog_write("INFO", "VFS mounted ramfs root");
    task_init(hal_ticks());
    register_platform_devices();
    klog_write("INFO", "device manager initialized");
    if (framebuffer_status()->active != 0u && boot_option_present("mvh.mode=setup"))
        desktop_run(1u);
    if (framebuffer_status()->active != 0u && boot_option_present("mvh.mode=desktop"))
        desktop_run(0u);
    console_colored("+==================================================+\n", 0x0Bu);
    console_colored("|                MVH Kernel x86_64                 |\n", 0x0Fu);
    console_colored("|         Version " MVH_KERNEL_VERSION " - MVHCLOUD.com             |\n", 0x0Fu);
    console_colored("+==================================================+\n", 0x0Bu);
    console_write("MVH kernel ready\n");
    console_write("\nType 'help' for available commands.\n");
    console_write("System monitor: statics\n\n");
    show_prompt();
    for (;;) {
        uint64_t input_entropy;
        input = hal_keyboard_read();
        input_entropy = cpu_read_tsc() ^ (hal_ticks() << 8u) ^ (uint8_t)input;
        entropy_add(&input_entropy, sizeof(input_entropy), 1u);
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
