#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![feature(llvm_asm)]
#![feature(integer_atomics)]
#![feature(thread_local)]
#![feature(new_uninit)]
#![feature(get_mut_unchecked)]

extern crate byteorder;

use std::cell::UnsafeCell;
use std::ffi::CString;
use std::mem;
use std::os::raw::{c_int, c_void};
use std::sync::atomic::{AtomicI32, Ordering};
use std::sync::Arc;
use std::time::Duration;

pub mod ffi {
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

mod asm;
pub mod storage;
pub mod tcp;
pub mod cxltp;
pub mod thread;
pub mod udp;

pub use asm::*;

fn convert_error(ret: c_int) -> Result<(), i32> {
    if ret == 0 {
        Ok(())
    } else {
        Err(ret as i32)
    }
}

#[inline]
pub fn preempt_enable() {
    unsafe {
        llvm_asm!("" ::: "memory" : "volatile");
        llvm_asm!("subl $$1, %fs:preempt_cnt@tpoff" : : : "memory", "cc" : "volatile");
        if ffi::preempt_cnt == 0 {
            ffi::preempt();
        }
    }
}

#[inline]
pub fn preempt_disable() {
    unsafe {
        llvm_asm!("addl $$1, %fs:preempt_cnt@tpoff" : : : "memory", "cc" : "volatile");
        llvm_asm!("" ::: "memory" : "volatile");
    }
}

#[allow(unused)]
pub fn base_init() -> Result<(), i32> {
    let res: i32 = 0;
    convert_error(res)
}

#[allow(unused)]
pub fn base_init_thread() -> Result<(), i32> {
    let res: i32 = 0;
    convert_error(res)
}

pub fn delay_us(microseconds: u64) {
    unsafe { ffi::__time_delay_us(microseconds) }
}

#[inline]
pub fn microtime() -> u64 {
    unsafe { (rdtsc() - ffi::start_tsc as u64) / ffi::cycles_per_us as u64 }
}

pub fn sleep(duration: Duration) {
    unsafe {
        ffi::timer_sleep(duration.as_secs() * 1000_000 + duration.subsec_nanos() as u64 / 1000)
    }
}

pub fn runtime_init<F>(cfgpath: String, f: F) -> Result<(), i32>
where
    F: FnOnce(),
    F: Send + 'static,
{
    convert_error(unsafe {
        ffi::runtime_init(
            CString::new(cfgpath).unwrap().into_raw(),
            Some(thread::box_trampoline::<F>),
            Box::into_raw(Box::new(f)) as *mut c_void,
        )
    })
}

#[derive(Clone)]
pub struct WaitGroup {
    inner: Arc<ffi::waitgroup>,
}
impl WaitGroup {
    pub fn new() -> Self {
        let mut inner_uninit = Arc::new_uninit();
        unsafe { ffi::reentrant_waitgroup_init(Arc::get_mut_unchecked(&mut inner_uninit).as_mut_ptr()) };
        let inner = unsafe { inner_uninit.assume_init() };
        Self { inner }
    }
    pub fn add(&self, count: i32) {
        unsafe { ffi::reentrant_waitgroup_add(&*self.inner as *const _ as *mut _, count as c_int) }
    }
    pub fn wait(&self) {
        unsafe { ffi::reentrant_waitgroup_wait(&*self.inner as *const _ as *mut _) }
    }
    pub fn done(&self) {
        self.add(-1)
    }
}
unsafe impl Send for WaitGroup {}
unsafe impl Sync for WaitGroup {}

pub struct SpinLock {
    inner: UnsafeCell<ffi::spinlock_t>,
}
impl SpinLock {
    pub fn new() -> Self {
        Self {
            inner: UnsafeCell::new(ffi::spinlock_t { locked: 0 }),
        }
    }

    #[inline]
    unsafe fn as_atomic(&self) -> &mut AtomicI32 {
        mem::transmute(&mut (*self.inner.get()).locked)
    }

    #[inline]
    fn as_raw(&self) -> *mut ffi::spinlock_t {
        self.inner.get()
    }

    #[inline]
    pub fn lock_np(&self) {
        preempt_disable();
        self.lock();
    }

    #[inline]
    pub fn unlock_np(&self) {
        self.unlock();
        preempt_enable();
    }

    #[inline]
    pub fn lock(&self) {
        let inner = unsafe { self.as_atomic() };
        while inner.swap(1, Ordering::Acquire) != 0 {
            while inner.load(Ordering::Relaxed) != 0 {
                cpu_relax();
            }
        }
    }

    #[inline]
    pub fn try_lock(&self) -> bool {
        let inner = unsafe { self.as_atomic() };
        inner.swap(1, Ordering::Acquire) == 0
    }

    #[inline]
    pub fn unlock(&self) {
        let inner = unsafe { self.as_atomic() };
        assert_eq!(inner.swap(0, Ordering::Release), 1);
    }
}
unsafe impl Send for SpinLock {}
unsafe impl Sync for SpinLock {}

#[derive (Copy, Clone)]
pub struct VTime {
    start_tsc: u64,
}
impl VTime {
    pub fn now() -> Self {
        Self {
            start_tsc: unsafe {rdtsc()},
        }
    }

    pub fn elapsed(&self) -> Duration {
        Duration::from_micros( 
            unsafe { (rdtsc() - self.start_tsc) / ffi::cycles_per_us as u64} 
        )
    }
}
unsafe impl Send for VTime {}
unsafe impl Sync for VTime {}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn spinlock() {
        let lock = SpinLock::new();

        lock.lock();
        assert!(!lock.try_lock());
        lock.unlock();
        assert!(lock.try_lock());
    }
}
