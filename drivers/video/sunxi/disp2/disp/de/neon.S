#ifndef EPDC_TESTCASE

#include <linux/linkage.h>
#include <asm/assembler.h>

#else

.macro _entry sym
  .global \sym
  \sym:
.endm
#define ENTRY(sym) _entry sym

#endif

#define   q0l   d0
#define   q0h   d1
#define   q1l   d2
#define   q1h   d3
#define   q2l   d4
#define   q2h   d5
#define   q3l   d6
#define   q3h   d7
#define   q4l   d8
#define   q4h   d9
#define   q5l  d10
#define   q5h  d11
#define   q6l  d12
#define   q6h  d13
#define   q7l  d14
#define   q7h  d15
#define   q8l  d16
#define   q8h  d17
#define   q9l  d18
#define   q9h  d19
#define  q10l  d20
#define  q10h  d21
#define  q11l  d22
#define  q11h  d23
#define  q12l  d24
#define  q12h  d25
#define  q13l  d26
#define  q13h  d27
#define  q14l  d28
#define  q14h  d29
#define  q15l  d30
#define  q15h  d31

.macro ENQ_PROCESS ra,rb

	vld2.32 { q8l[0],q9l[0] }, [r2],r3
	vld2.32 { q8l[1],q9l[1] }, [r2],r3
	vld2.32 { q8h[0],q9h[0] }, [r2],r3
	vshr.u8 \ra, \ra, #4
	vld2.32 { q8h[1],q9h[1] }, [r2],r1
	vshr.u8 \rb, \rb, #4

	veor.u8 q10, q8, \ra
	vshl.u8 q8, q8, #4
	vorr.u8 q11, q11, q10
	veor.u8 q10, q9, \rb
	vshl.u8 q9, q9, #4
	vorr.u8 q11, q11, q10
	vorr.u8 q8, q8, \ra
	vorr.u8 q9, q9, \rb

	vst2.32 { q8l[0],q9l[0] }, [r2],r3
	vst2.32 { q8l[1],q9l[1] }, [r2],r3
	vst2.32 { q8h[0],q9h[0] }, [r2],r3
	vst2.32 { q8h[1],q9h[1] }, [r2],r3

.endm
	
ENTRY(epdc_neon_init)

	vstmia r0!, { q0-q7 }
	vstmia r0!, { q8-q15 }
	bx lr

ENTRY(epdc_neon_complete)

	vldmia r0!, { q0-q7 }
	vldmia r0!, { q8-q15 }
	bx lr

@ void epdc_neon_cw_step(void *fbptr, uint32_t fbscanline, void *imapptr, uint32_t imapscanline)
@ void epdc_neon_ccw_step(void *fbptr, uint32_t fbscanline, void *imapptr, uint32_t imapscanline)
@
@ r0: framebuffer pointer
@ r1: framebuffer scanline
@ r2: indexmap pointer
@ r3: indexmap scanline

ENTRY(epdc_neon_cw_step)

  vld4.32 { q0l[0],q1l[0],q2l[0],q3l[0] }, [r0],r1
  vld4.32 { q0l[1],q1l[1],q2l[1],q3l[1] }, [r0],r1
  vld4.32 { q0h[0],q1h[0],q2h[0],q3h[0] }, [r0],r1
  vld4.32 { q0h[1],q1h[1],q2h[1],q3h[1] }, [r0],r1

  vld4.32 { q4l[0],q5l[0],q6l[0],q7l[0] }, [r0],r1
  vld4.32 { q4l[1],q5l[1],q6l[1],q7l[1] }, [r0],r1
  vld4.32 { q4h[0],q5h[0],q6h[0],q7h[0] }, [r0],r1
  vld4.32 { q4h[1],q5h[1],q6h[1],q7h[1] }, [r0],r1

	b epdc_enqueue_z

ENTRY(epdc_neon_ccw_step)

  vld4.32 { q4h[1],q5h[1],q6h[1],q7h[1] }, [r0],r1
  vld4.32 { q4h[0],q5h[0],q6h[0],q7h[0] }, [r0],r1
  vld4.32 { q4l[1],q5l[1],q6l[1],q7l[1] }, [r0],r1
  vld4.32 { q4l[0],q5l[0],q6l[0],q7l[0] }, [r0],r1

  vld4.32 { q0h[1],q1h[1],q2h[1],q3h[1] }, [r0],r1
  vld4.32 { q0h[0],q1h[0],q2h[0],q3h[0] }, [r0],r1
  vld4.32 { q0l[1],q1l[1],q2l[1],q3l[1] }, [r0],r1
  vld4.32 { q0l[0],q1l[0],q2l[0],q3l[0] }, [r0],r1

epdc_enqueue_z:

	vmov.i8 q11, #0

	movs r1, #0

  vzip.8 q0l, q0h
  vzip.8 q1l, q1h
  vzip.8 q2l, q2h
  vzip.8 q3l, q3h
  vzip.8 q0l, q0h
  vzip.8 q1l, q1h
  vzip.8 q2l, q2h
  vzip.8 q3l, q3h

	subs r1, r1, r3

  vzip.8 q4l, q4h
  vzip.8 q5l, q5h
  vzip.8 q6l, q6h
  vzip.8 q7l, q7h
  vzip.8 q4l, q4h
  vzip.8 q5l, q5h
  vzip.8 q6l, q6h
  vzip.8 q7l, q7h

	subs r1, r1, r3,LSL#1

	ENQ_PROCESS q0,q4
	ENQ_PROCESS q1,q5
	ENQ_PROCESS q2,q6
	ENQ_PROCESS q3,q7

	vorr q11l, q11l, q11h
	vmov r0, r1, q11l
  orrs r0, r0, r1
	bx lr


