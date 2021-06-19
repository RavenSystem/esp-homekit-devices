extern void wDev_ProcessFiq(void);

void call_wdev(void)
{
    wDev_ProcessFiq();
}

