#include "patch_file.h"

// This patch applies the FIRM protection code needed for safe a9lh usage.

/*
  rel p9_exefs
  find 4, "exe:"
  back 0x1
  back 0xff
  find 4, 0x00, 0x28, 0x01, 0xDA
  set 4, 0x00, 0x20, 0xC0, 0x46
 */

extern exefs_h* firm_p9_exefs;

PATCH(firmprot)
{
    uint8_t* firm_mem = (uint8_t*)firm_p9_exefs + sizeof(exefs_h) +
                        firm_p9_exefs->fileHeaders[0].offset;
    uint32_t size = firm_p9_exefs->fileHeaders[0].size;

    // We look for 'exe:' first; this string is close to what we patch
    uint8_t* off = memfind(firm_mem, size, (uint8_t*)"exe:", 4);

    if (off == NULL) {
        fprintf(stderr, "Couldn't find 'exe:' string.\n");
        return 1;
    }

    fprintf(stderr, "Firmprot: 'exe:' string @ %x\n", (uint32_t)off);

    uint8_t pattern[] = { 0x00, 0x28, 0x01, 0xDA };

    uint8_t* firmprot = memfind(off - 0x100, 0x100, pattern, 4);

    if (firmprot == NULL) {
        fprintf(stderr, "Couldn't find firmprot code.\n");
        return 2;
    }

    fprintf(stderr, "Firmprot: %x\n", (uint32_t)firmprot);

    uint8_t patch[] = { 0x00, 0x20, 0xC0, 0x46 };
    memcpy(firmprot, patch, 4);

    fprintf(stderr, "Applied firmprot patch.\n");

    return 0;
}
