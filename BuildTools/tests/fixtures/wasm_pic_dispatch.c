void (*dispatch)(void);
void implementation(void);

void invoke(void)
{
    void (*local)(void) = implementation;
    __atomic_store(&dispatch, &local, 5);
    __atomic_thread_fence(5);
    dispatch();
}
