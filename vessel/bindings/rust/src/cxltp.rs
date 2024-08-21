use std::io::{self, Read, Write};
use std::ptr;
use std::net::{Ipv4Addr, SocketAddrV4};

use byteorder::{ByteOrder, NetworkEndian};

use super::*;


fn isize_to_result(i: isize) -> io::Result<usize> {
    if i >= 0 {
        Ok(i as usize)
    } else {
        Err(io::Error::from_raw_os_error(i as i32))
    }
}

pub struct CxltpQueue(*mut ffi::cxltpqueue_t);
impl CxltpQueue {
    pub fn listen(backlog: i32) -> io::Result<Self> {
        let mut queue = ptr::null_mut();
        let laddr = ptr::null_mut();
        let ret = unsafe { ffi::cxltp_listen(laddr, backlog as usize, &mut queue as *mut _) };
        if ret < 0 {
            Err(io::Error::from_raw_os_error(ret as i32))
        } else {
            Ok(CxltpQueue(queue))
        }
    }
    pub fn accept(&self) -> io::Result<CxltpConnection> {
        let mut conn = ptr::null_mut();
        let ret = unsafe { ffi::cxltp_accept(self.0, &mut conn as *mut _) };
        if ret < 0 {
            Err(io::Error::from_raw_os_error(ret as i32))
        } else {
            Ok(CxltpConnection(conn))
        }
    }
    pub fn shutdown(&self) {
        unsafe { ffi::cxltp_qshutdown(self.0) }
    }
}
impl Drop for CxltpQueue {
    fn drop(&mut self) {
        unsafe { ffi::cxltp_qclose(self.0) }
    }
}
unsafe impl Send for CxltpQueue {}
unsafe impl Sync for CxltpQueue {}

pub struct CxltpConnection(*mut ffi::cxltpconn_t);
impl CxltpConnection {
    pub fn dial(remote_addr: u64, port: u64) -> io::Result<Self> {
        let raddr = ffi::cxltp_addr_t {
            base: remote_addr as *mut c_void,
            port: port,
        };
        let mut conn = ptr::null_mut();
        
        let ret = unsafe { ffi::cxltp_dial(raddr, &mut conn as *mut _) };
        if ret < 0 {
            Err(io::Error::from_raw_os_error(ret as i32))
        } else {
            Ok(CxltpConnection(conn))
        }
    }

    pub fn local_addr(&self) -> SocketAddrV4 {
        SocketAddrV4::new(Ipv4Addr::new(127, 0, 0, 1), 0)
    }

    pub fn remote_addr(&self) -> SocketAddrV4 {
        SocketAddrV4::new(Ipv4Addr::new(127, 0, 0, 1), 0)
    }

    pub fn shutdown(&self) -> io::Result<()> {
        let res = unsafe { ffi::cxltp_shutdown(self.0) };
        if res == 0 {
            Ok(())
        } else {
            Err(io::Error::from_raw_os_error(res as i32))
        }
    }

    pub fn abort(&self) {
        unsafe { ffi::cxltp_abort(self.0) };
    }
}

impl<'a> Read for &'a CxltpConnection {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        isize_to_result(unsafe {
            ffi::cxltp_read(self.0, buf.as_mut_ptr() as *mut c_void, buf.len())
        })
    }
}
impl Read for CxltpConnection {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        isize_to_result(unsafe {
            ffi::cxltp_read(self.0, buf.as_mut_ptr() as *mut c_void, buf.len())
        })
    }
}
impl<'a> Write for &'a CxltpConnection {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        isize_to_result(unsafe { ffi::cxltp_write(self.0, buf.as_ptr() as *const c_void, buf.len()) })
    }
    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}
impl Write for CxltpConnection {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        isize_to_result(unsafe { ffi::cxltp_write(self.0, buf.as_ptr() as *const c_void, buf.len()) })
    }
    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}
impl Drop for CxltpConnection {
    fn drop(&mut self) {
        unsafe { ffi::cxltp_close(self.0) }
    }
}
unsafe impl Send for CxltpConnection {}
unsafe impl Sync for CxltpConnection {}
