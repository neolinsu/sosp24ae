SECTIONS
{
  __perthread_load = .;
  .perthread 0 : AT(__perthread_load) {
    PROVIDE(__perthread_start = .);
    *(.perthread)
    PROVIDE(__perthread_end = .);
  }
  . = __perthread_load + SIZEOF(.perthread);
}
INSERT AFTER .text
