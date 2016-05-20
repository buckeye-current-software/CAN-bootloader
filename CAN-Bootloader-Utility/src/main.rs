extern crate libc;
use libc::*;
use std::io;
use std::io::prelude::*;
use std::fs::File;
use std::io::BufReader;
use std::borrow::ToOwned;
use std::env;
use std::time::Duration;
use std::thread;

#[link(name = "canlib32")]
extern {
	fn canOpenChannel(ctrl: u16, flags: u16) -> i16;
	fn canInitializeLibrary();
	fn canSetBusParams(handle: i16, bitrate: i32, tseg1: u16, tseg2: u16, sjw: u16, noSamp: u16, syncmode: u16) -> i16;
	fn canWriteWait(handle: i16, id: u32, msg: *const c_void, dlc: u16, flag: u16, timeout: u32) -> i16;
	fn canWrite(handle: i16, id: u32, msg: *const c_void, dlc: u16, flag: u16) -> i16;
    fn canWriteSync(handle: i16, timeout: u32) -> i16;
	fn canBusOn(handle: i16) -> i16;
	fn canClose(handle: i16) -> i16;
	fn canReadWait(handle: i16, id: *mut i32, msg: *mut c_void, dlc: *mut u16, flag: *mut u16, time: *mut u32, timeout: u32) -> i16;
	fn canFlushReceiveQueue(handle: i16) -> i16;
	fn canReadSyncSpecific(handle: i16, id: i32, timeout: u32) -> i16;
}

fn main() {
    // CAN library initialization
	let mut fileParam = String::from("");
	let mut deviceParam = 0;
	let mut bypassCmdStart = 0;
	// Determine arguments
	let args: Vec<_> = env::args().collect();
	for index in 0..args.len() {
		if (args[index] == "-i") && (index + 1 < args.len()){
			fileParam = args[index + 1].to_string();
		}
		else if (args[index] == "-d") && (index + 1 < args.len()) {
			match args[index + 1].parse::<u32>() {
				Ok(n) => deviceParam = n,
				Err(e) => {
					println!("Unable to parse -d. Error: {}", e);
					return
				}
			}
		}
		else if (args[index] == "-b") {
			bypassCmdStart = 1;
		}
	}
	
	println!("File: {}, Dev: {}", fileParam, deviceParam);
	
	unsafe {canInitializeLibrary()};
	
	let hndl = unsafe {canOpenChannel(0, 0)};
	
	if hndl < 0 {
		println!("Failed to open CAN channel!. Error: {}", hndl);
		return
	}
	
	let bitrate = -1; // 500 Kb/sec
	let mut result = unsafe {canSetBusParams(hndl, bitrate, 0, 0, 0, 0, 0)};
	
	if result != 0 {
		println!("Failed to set CAN bus parameters. Error: {}", result);
		return
	}
	
	result = unsafe {canBusOn(hndl)};
	if result != 0 {
		println!("Failed to go bus on. Error: {}", result);
		return
	}
	
	
	// Attempt to read program hex file
	let mut f =  match File::open(fileParam) {
		Ok(f) => f,
		Err(e) => {
			println!("Unable to open program file. Error: {}", e);
			return
		}
	};
	let mut complete = 0;
	
	if ((deviceParam != 0) && (bypassCmdStart == 0))
	{
		let mut bootloadStartCmd: [u8; 8] = [0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF];
		let result = unsafe {canWriteWait(hndl, deviceParam, bootloadStartCmd.as_mut_ptr() as *mut c_void, 4, 0, 10000)};
		if result != 0 {
			println!("Unable to send start CAN bootload message. Error: {}", result);
			return
		}
	}
	else if (bypassCmdStart == 0)
	{
		println!("Device param -d can not be 0. Quitting!");
		return
	}
	else
	{
		println!("Bootload start command bypassed!");
	}
	
	while complete == 0 {

		// Wait for message that device bootload is ready for program
		let mut rx_bytes: [u8; 8] = [0, 0, 0, 0, 0, 0, 0, 0];
		let mut id = 0;
		let mut dlc = 0;
		let mut flag = 0;
		let mut time = 0;
		let mut start = 0;
		while start == 0 {
			result = unsafe{canReadWait(hndl, &mut id, rx_bytes.as_mut_ptr() as *mut c_void, &mut dlc, &mut flag, &mut time, 0xFFFFFFFF)};
			println!("Result: {}, id {}, flag {}, dlc {}, time {}", result, id, flag, dlc, time);
			if (result == 0) && (flag == 2) {
				start = 1;
			}
		}
		println!("Found bootload heartbeat! Started bootload!");
		unsafe{canFlushReceiveQueue(hndl)};

		// Start sending program to bootloader
		let mut file_contents = BufReader::new(&f);
		let mut bytes: [u8; 4] = [0, 0, 0, 0];
		let mut index = 0;
		let mut count: u16 = 0;

		for byte in file_contents.bytes() {
			let content = byte.unwrap();
			//println!("Byte: {}", content);
			if (content >= 48)		// If not STX or ETX
			{
				bytes[index] = convert_ascii_to_hex(content);
				index += 1;
				if index >= 4 {
					index = 0;
					count = count + 1;
					can_send_stream(hndl, (bytes[0] << 4) + bytes[1], (bytes[2] << 4) + bytes[3], count);
				}
			}
		}

		result = unsafe{canReadSyncSpecific(hndl, 2, 10000)};
		if result == 0
		{
			result = unsafe{canReadWait(hndl, &mut id, rx_bytes.as_mut_ptr() as *mut c_void, &mut dlc, &mut flag, &mut time, 10000)};
			// Successful program message received. Bootloading complete
			if (result == 0) && (rx_bytes[2] == 128){
				complete = 1;
				println!("Bootloading completed successfully!");
			}
			else {
				println!("Bootloading failed! Waiting for bootload heartbeat for retry ...");
			}
		}
	}
	result = unsafe {canClose(hndl)};
		if result != 0 {
		println!("Failed to close bus. Error: {}", result);
		return
	}
	
}

fn convert_ascii_to_hex(ascii_char: u8) -> u8
{
	if ascii_char > 64 {
		return ascii_char - 55
	}
	else {
		return ascii_char - 48
	}
}

fn can_send_stream(handle: i16, byte1: u8, byte2: u8, count: u16)
{
	let mut msg_data: [u8; 4] = [(count >> 8) as u8, count as u8, byte1, byte2];
	let result = unsafe {canWriteWait(handle, 1, msg_data.as_mut_ptr() as *mut c_void, 4, 0, 10000)};
	while result != 0 {
		println!("Failed to send CAN message: {}, {}", byte1, byte2);
		let result = unsafe {canWriteWait(handle, 1, msg_data.as_mut_ptr() as *mut c_void, 4, 0, 10000)};
	}
	//thread::sleep(Duration::from_millis(10));
}