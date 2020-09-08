#ifndef _DISP_DEBUG_H
#define _DISP_DEBUG_H

#include "linux/clk-provider.h"


#define ERR(x...)  { printk(x); }
#define INF(x...)  { printk(x); }
#define DBG(x...)  { printk(x); }
#define DDBG(x...) { printk(x); }

#define XLOG1(x...) printk(x)

static inline int x_clk_set_rate(struct clk *clk, unsigned long rate)
{
        DDBG("  clk_set_rate(%s,%lu)\n", __clk_get_name(clk), rate)
        return clk_set_rate(clk, rate);
}

static inline unsigned long x_clk_get_rate(struct clk *clk)
{
        unsigned long rate = __clk_get_rate(clk);
        DDBG("  clk_get_rate(%s)=%lu\n", __clk_get_name(clk), rate);
        return rate;
}

static inline struct clk *x_clk_get_parent(struct clk *clk)
{
        struct clk *parent = __clk_get_parent(clk);
        DDBG("  clk_get_parent(%s)=%s\n", __clk_get_name(clk), parent ?  __clk_get_name(parent) : "none");
        return parent;
}

#endif

