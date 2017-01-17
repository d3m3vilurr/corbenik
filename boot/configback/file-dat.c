#include <malloc.h>               // for memalign
#include <stdint.h>               // for uint8_t, uint32_t
#include <string.h>               // for memset, strncpy, memcmp, strcpy, NULL
#include "config.h"               // for SYSCONFDIR, BINDIR, LIBDIR, LIBEXECDIR
#include "ctr9/io/fatfs/ff.h"     // for f_mkdir, f_unlink
#include "ctr9/io/sdmmc/sdmmc.h"  // for sdmmc_get_cid
#include "menu.h"                 // for add_patch_menu, poweroff, reset_pat...
#include "option.h"               // for config_file, OPTION_ACCENT_COLOR
#include "std/fs.h"               // for cropen, crclose, crwrite, crread
#include "structures.h"           // for PATH_AGB_F, PATH_AUX_PATCHES, PATH_...

FILE *conf_handle;

struct config_file *config;
extern uint8_t *enable_list;
char *config_file_path = NULL;
int changed_consoles = 0;
uint32_t cid[4];

#define PATCH_MAX 0x100000

void
regenerate_config(void)
{
    for(int i=0; i < 4; i++)
        config->magic[i] = CONFIG_MAGIC[i];

    config->config_ver = config_version;
    config->options[OPTION_ACCENT_COLOR] = 2;
    config->options[OPTION_BRIGHTNESS]   = 3;

    if (!(conf_handle = cropen(config_file_path, "w")))
        poweroff();

    crwrite(config, 1, sizeof(struct config_file) + PATCH_MAX, conf_handle);
    crclose(conf_handle);
}

void
mk_structure(void)
{
    f_mkdir(PREFIX);
      f_mkdir(LIBEXECDIR);
      f_mkdir(LIBDIR);
        f_mkdir(PATH_EXEFS);
          f_mkdir(PATH_EXEFS_TEXT);
          f_mkdir(PATH_EXEFS_DATA);
          f_mkdir(PATH_EXEFS_RO);
        f_mkdir(PATH_FIRMWARES);
        f_mkdir(PATH_MODULES);
      f_mkdir(BINDIR);
#if defined(CHAINLOADER) && CHAINLOADER == 1
      f_mkdir(PATH_CHAINS);
#endif
      f_mkdir(SBINDIR);
      f_mkdir(SYSCONFDIR);
      f_mkdir(LOCALSTATEDIR);
        f_mkdir(PATH_TEMP);
          f_mkdir(PATH_LOADER_CACHE);
        f_mkdir(PATH_LOG);
      f_mkdir(SHAREDIR);
        f_mkdir(PATH_KEYS);
        f_mkdir(LOCALEDIR);
          f_mkdir(PATH_LOCEMU);
}

void
update_config(void)
{
    int updated = 0;

    if (config->options[OPTION_ACCENT_COLOR] == 0) {
        config->options[OPTION_ACCENT_COLOR] = 2;
        updated = 1;
    }

    strncpy(config->firm[0], PATH_NATIVE_F, 255);
    strncpy(config->firm[1], PATH_TWL_F, 255);
    strncpy(config->firm[2], PATH_AGB_F, 255);

    if (updated) {
        save_config(); // Save the configuration.
    }
}

void get_cfg_path(void);

void
load_config(void)
{
    mk_structure(); // Make directory structure if needed.

    if (!config_file_path) {
        config_file_path = memalign(16, 256); // MAX_PATH
        memset(config_file_path, 0, 256);

        sdmmc_get_cid(1, cid);

        FILE* f = cropen(SYSCONFDIR "/current-nand-cid", "r");
        if (!f) {
            // Nonexistent. Write it.
            f = cropen(SYSCONFDIR "/current-nand-cid", "w");
            crwrite(cid, 1, 4, f);
            crclose(f);
            f = cropen(SYSCONFDIR "/current-nand-cid", "r");
        }

        crread(&cid[1], 1, 4, f);

        // If our console's CID doesn't match what was read, we need to regenerate caches immediately when we can.
        if (cid[0] != cid[1]) {
            changed_consoles = 1;
        }

        strcpy(config_file_path, SYSCONFDIR "/config-00000000");

        static const char hexDigits[] = "0123456789ABCDEF";
        char* cfg = config_file_path;
        uint32_t cid_cp = cid[0];
        while(cfg[1] != 0) cfg++;

        // Get path of actual config.
        while (cid_cp) {
            cfg[0] = hexDigits[(uint32_t)(cid_cp & 0xF)];
            cfg--;
            cid_cp >>= 4;
        }

        config = (struct config_file*)memalign(16, sizeof(struct config_file) + PATCH_MAX);
        memset(config, 0, sizeof(struct config_file) + PATCH_MAX);
        enable_list = (uint8_t*)config + sizeof(struct config_file);
        crclose(f);
    }

    // Zero on success.
    if (!(conf_handle = cropen(config_file_path, "r"))) {
        regenerate_config();
    } else {
        crread(config, 1, sizeof(struct config_file) + PATCH_MAX, conf_handle);

        crclose(conf_handle);

        if (memcmp(&(config->magic), CONFIG_MAGIC, 4)) {
            f_unlink(config_file_path);
            regenerate_config();
        }

        if (config->config_ver != config_version) {
            f_unlink(config_file_path);
            regenerate_config();
        }
    }

    reset_patch_menu();
    add_patch_menu(PATH_PATCHES);
    add_patch_menu(PATH_AUX_PATCHES);

    update_config();
}

void
save_config(void)
{
    if (changed_consoles) {
        FILE* f = cropen(SYSCONFDIR "/current-nand-cid", "w");
        crwrite(cid, 1, 4, f);
        crclose(f);
    }

    f_unlink(config_file_path);

    if (!(conf_handle = cropen(config_file_path, "w")))
        while(1);

    crwrite(config, 1, sizeof(struct config_file) + PATCH_MAX, conf_handle);

    crclose(conf_handle);
}

void toggle_opt(void* val) {
    uint32_t opt = (uint32_t)val;
    uint8_t* set = & (config->options[opt]);
    switch(opt) {
        case OPTION_EMUNAND_INDEX:
            // 0-9
            set[0]++;
            if (set[0] > 9)
                set[0] = 0;
            break;
        case OPTION_BRIGHTNESS:
            // 0-3
            set[0]++;
            if (set[0] > 3)
                set[0] = 0;
            break;
        case OPTION_ACCENT_COLOR:
            // 1-7
            set[0]++;
            if (set[0] > 7 || set[0] < 1)
                set[0] = 1;
            break;
        default:
            set[0] = !(set[0]);
            break;
    }
}

char* get_opt(void* val) {
    uint32_t opt = (uint32_t)val;
    if (opt >= OPTION_NFIRM_PATH && opt <= OPTION_AFIRM_PATH) {
        opt -= OPTION_NFIRM_PATH;
        return config->firm[opt];
    }

    char raw = config->options[opt];
    static char str[2] = "0";
    str[0] = '0';
    switch(opt) {
        case OPTION_EMUNAND_INDEX:
        case OPTION_BRIGHTNESS:
        case OPTION_ACCENT_COLOR:
            str[0] += raw;
            break;
        default:
            if (raw)
                str[0] = '*';
            else
                str[0] = ' ';
            break;
    }
    return str;
}

uint32_t get_opt_u32(uint32_t val) {
    uint32_t opt = config->options[(uint32_t)val];

    return opt;
}

int set_opt_u32(uint32_t key, uint32_t val) {
    if (key >= OPTION_NFIRM_PATH && key <= OPTION_AFIRM_PATH)
        return 1;

    config->options[(uint32_t)key] = (uint8_t)val;

    return 0;
}

int set_opt_str(uint32_t key, const char* val) {
    if (!(key >= OPTION_NFIRM_PATH && key <= OPTION_AFIRM_PATH))
        return 1;

    key -= OPTION_NFIRM_PATH;

    strncpy(config->firm[key], val, 255);

    return 0;
}
