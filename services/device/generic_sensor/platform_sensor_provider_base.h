// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_BASE_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_BASE_H_

#include "base/macros.h"

#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "services/device/generic_sensor/platform_sensor.h"

namespace device {

// Base class that defines factory methods for PlatformSensor creation.
// Its implementations must be accessed via GetInstance() method.
class PlatformSensorProviderBase {
 public:
  using CreateSensorCallback =
      base::Callback<void(scoped_refptr<PlatformSensor>)>;

  // Creates new instance of PlatformSensor.
  void CreateSensor(mojom::SensorType type,
                    const CreateSensorCallback& callback);

  // Gets previously created instance of PlatformSensor by sensor type |type|.
  scoped_refptr<PlatformSensor> GetSensor(mojom::SensorType type);

  // Shared buffer getters.
  mojo::ScopedSharedBufferHandle CloneSharedBufferHandle();

  // Returns 'true' if some of sensor instances produced by this provider are
  // alive; 'false' otherwise.
  bool HasSensors() const;

  // Implementations might want to override this in order to be able
  // to read from sensor files. For example, linux does so.
  virtual void SetFileTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner) {}

 protected:
  PlatformSensorProviderBase();
  virtual ~PlatformSensorProviderBase();

  // Method that must be implemented by platform specific classes.
  virtual void CreateSensorInternal(mojom::SensorType type,
                                    mojo::ScopedSharedBufferMapping mapping,
                                    const CreateSensorCallback& callback) = 0;

  // Implementations might override this method to free resources when there
  // are no sensors left.
  virtual void AllSensorsRemoved() {}

  void NotifySensorCreated(mojom::SensorType type,
                           scoped_refptr<PlatformSensor> sensor);

  std::vector<mojom::SensorType> GetPendingRequestTypes();

  mojo::ScopedSharedBufferMapping MapSharedBufferForType(
      mojom::SensorType type);

  THREAD_CHECKER(thread_checker_);

 private:
  friend class PlatformSensor;  // To call RemoveSensor();

  bool CreateSharedBufferIfNeeded();
  void RemoveSensor(mojom::SensorType type);

 private:
  using CallbackQueue = std::vector<CreateSensorCallback>;

  std::map<mojom::SensorType, PlatformSensor*> sensor_map_;
  std::map<mojom::SensorType, CallbackQueue> requests_map_;
  mojo::ScopedSharedBufferHandle shared_buffer_handle_;

  DISALLOW_COPY_AND_ASSIGN(PlatformSensorProviderBase);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_BASE_H_
