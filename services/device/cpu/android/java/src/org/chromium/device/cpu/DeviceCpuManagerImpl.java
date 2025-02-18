// Copyright 2014 The Chromium Authors. All rights reserved.
// Copyright (C) 2016 INFRAWARE, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.cpu;
import org.chromium.device.mojom.DeviceCpuManager;
import org.chromium.device.mojom.DeviceCpuResultCode;
import android.content.Context;
import java.lang.Exception;
import java.util.StringTokenizer;
import android.util.Log;

import java.io.RandomAccessFile;
import java.io.IOException;

import org.chromium.base.ContextUtils;
import org.chromium.mojo.system.MojoException;
import org.chromium.services.service_manager.InterfaceFactory;


/**
 * Android implementation of the devicecpu manager service defined in
 * device/cpu/devicecpu_manager.mojom.
 */
public class DeviceCpuManagerImpl implements DeviceCpuManager {
	private static final String TAG = "chromium";
	private Context mContext;
	private Thread checkCpuThread = null;
	private float cpuLoad = 0;
	private boolean threadContinue = false;

	static class device_cpu_ErrorCodeList{
		// Exception code
		static final int SUCCESS = 0;
		static final int FAILURE = -1;
		static final int NOT_ENABLED_PERMISSION = -2;
		static final int NOT_SUPPORT_API = 9999;
	};

	static class device_cpu_function {
		static final int FUNC_GET_CPU_LOAD = 0;
	};

	public DeviceCpuManagerImpl(Context context) {
		mContext = context;
	}

	@Override
	public void close() {
		threadContinue = false;
		checkCpuThread = null;
	}

	@Override
	public void onConnectionError(MojoException e) {
		threadContinue = false;
	}

	@Override
	public void getDeviceCpuLoad(GetDeviceCpuLoadResponse callback) {
		Thread callbackthread = new Thread(new callbackThread(callback, cpuLoad));
		callbackthread.start();
	}

	@Override
	public void startCpuLoad() {
		if (checkCpuThread != null && checkCpuThread.isAlive()) {
			return;
		} else {
			checkCpuThread = new Thread(new Runnable() {
				@Override
				public void run() {
					try {
						threadContinue = true;
						while(threadContinue) {
							Thread.sleep(500);
							cpuLoad = readUsage();
							if (cpuLoad == -1) {
								threadContinue = false;
							}
						}
					} catch (Exception e) {
						e.printStackTrace();
						cpuLoad = -1;
					}
				}
			});
			checkCpuThread.start();
		}
	}

	@Override
	public void stopCpuLoad() {
		threadContinue = false;
	}

	private float readUsage() {
		try {
			RandomAccessFile reader = new RandomAccessFile("/proc/stat", "r");
			String load = reader.readLine();

			String[] toks = load.split(" +");  // Split on one or more spaces

			long idle1 = Long.parseLong(toks[4]) + Long.parseLong(toks[5]);
			long cpu1 = 0;
			for(int i = 1; i < toks.length; i++) {
				cpu1 += Long.parseLong(toks[i]);
			}
			try {
				Thread.sleep(1000);
			} catch (Exception e) {}

			reader.seek(0);
			load = reader.readLine();
			reader.close();

			toks = load.split(" +");

			long cpu2 = 0;
			long idle2 = Long.parseLong(toks[4]) + Long.parseLong(toks[5]);
			for(int i = 1; i < toks.length; i++) {
				cpu2 += Long.parseLong(toks[i]);
			}
			return 1 - (float)(idle2 - idle1) / (cpu2 - cpu1);

		} catch (IOException ex) {
			ex.printStackTrace();
			return -1;
		}
	}

	class callbackThread implements Runnable {
		GetDeviceCpuLoadResponse callback = null;
		float load = 0;

		callbackThread(GetDeviceCpuLoadResponse callback, float load) {
			this.callback = callback;
			this.load = load;
		}

		public void run() {
			DeviceCpuResultCode code = new DeviceCpuResultCode();
			code.resultCode = device_cpu_ErrorCodeList.FAILURE;
			code.functionCode = device_cpu_function.FUNC_GET_CPU_LOAD;

			try {
				Thread.sleep(1000);
			} catch (Exception e) {
				e.printStackTrace();
				return;
			}

			code.load = cpuLoad;
			if (cpuLoad == -1 ) {
				code.resultCode = device_cpu_ErrorCodeList.FAILURE;
			}
			else {
				code.resultCode = device_cpu_ErrorCodeList.SUCCESS;
			}

			callback.call(code);
			code = null;
		}
	}

	/**
	 * A factory for implementations of the AppLauncherManager interface.
	 */
	public static class Factory implements InterfaceFactory<DeviceCpuManager> {
			public Factory() {
			}

			@Override
			public DeviceCpuManager createImpl() {
					return new DeviceCpuManagerImpl(ContextUtils.getApplicationContext());
			}
	}

}
