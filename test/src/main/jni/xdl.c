#include "xdl.h"

#pragma clang diagnostic push
#pragma ide diagnostic ignored "bugprone-suspicious-include"
#include "external/xDL/xdl/src/main/cpp/xdl.c"
#pragma clang diagnostic pop

void *xdl_dsym_prefix(void *handle, const char *symbol, size_t *symbol_size) {
    if (NULL == handle || NULL == symbol) return NULL;
    if (NULL != symbol_size) *symbol_size = 0;

    xdl_t *self = (xdl_t *)handle;

    // load .symtab only once
    xdl_ensure_symtab_loaded(self);
    if (NULL == self->symtab) return NULL;

    // find symbol
    size_t symbol_len = strlen(symbol);
    for (size_t i = 0; i < self->symtab_cnt; i++) {
        ElfW(Sym) *sym = self->symtab + i;

        if (!XDL_SYMTAB_IS_EXPORT_SYM(sym->st_shndx)) continue;
        if (sym->st_name >= self->strtab_sz) continue;
        if (!xdl_dsym_is_match(self->strtab + sym->st_name, symbol, symbol_len) &&
            !xdl_util_starts_with(self->strtab + sym->st_name, symbol))
            continue;

        return xdl_resolve_symbol_address(self, sym, symbol_size);
    }

    return NULL;
}
