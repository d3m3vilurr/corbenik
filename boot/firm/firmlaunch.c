#include <stdint.h>         // for uint32_t, uint8_t
#include <stdlib.h>         // for free
#include <string.h>         // for memmove
#include <firm/headers.h>
#include <firm/keys.h>
#include <firm/firm.h>
#define FIRM_INTERNAL_CODE
#include <firm/internal.h>  // for void_call
#include "arm11.h"          // for deinitScreens
#include "firm/headers.h"   // for firm_section_h, firm_h
#include "std/draw.h"       // for crflush, stderr
#include "std/fs.h"         // for crumount

static volatile uint32_t *const a11_entry = (volatile uint32_t *)0x1FFFFFF8;

void firmlaunch(firm_h* firm) {
    // Get entrypoints
    uint32_t  entry11 = firm->a11Entry;
    void_call entry9  = (void_call)firm->a9Entry;

    // Copy sections from FIRMs to their destination.
    for (firm_section_h *section = firm->section; section < firm->section + 4 && section->address != 0; section++) {
        memmove((void *)section->address, (void *)((uint8_t*)firm + section->offset), section->size);
    }

    free(firm); // Really, no point in this. Why not, though.

    crflush(stderr); // Flush logs if need be before unmount.

    crumount(); // Unmount SD.

    deinitScreens(); // Turn off display

    *a11_entry = (uint32_t)entry11; // Start kernel11

    entry9(); // Start process9
}

