/*
 * Copyright 2023 wtcat 
 */
#include <iostream>

extern "C" void uiview_heap_user_print(void *fout, const void *user) {
    std::ostream *&fd = (std::ostream *&)fout;
    *fd << "ViewID(" << (uint16_t)(uint32_t)user << ")";
}
