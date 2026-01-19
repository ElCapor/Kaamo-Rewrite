namespace game {
    extern bool globals_initialized;
    extern bool yu_ready;
    extern bool quit;

    extern void* whitelisted_allocs[65536];
    bool is_whitelisted_alloc(void* ptr);
}