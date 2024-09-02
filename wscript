# Copyright 2023 wtcat 

import os
import rtems_waf.rtems as rtems

def build(bld):
    ccinclude = []
    ccinclude.append(os.path.join(bld.env.ROOTDIR, 'basework/os/rtems'))
    ccinclude.append(os.path.join(bld.env.ROOTDIR, 'basework/arch/arm'))

    rtems.build(bld)
    base_sources = [
        'log.c',
        'fsm.c',

        'container/kfifo.c',
        'container/observer.c',
        'container/radix-tree.c',
        'container/ring/rte_ring.c',

        'os/rtems/timer.c',
        'os/rtems/workq.c',
        'os/rtems/os_rtems.c'
    ]

    # Google test framework
    bld.env.CFLAGS += ccinclude
    bld.stlib(
        target = 'basework',
        source = base_sources,
    )
