// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/tts/arc_tts_service.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/speech/tts_controller.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"

namespace arc {
namespace {

// Singleton factory for ArcTtsService.
class ArcTtsServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcTtsService,
          ArcTtsServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcTtsServiceFactory";

  static ArcTtsServiceFactory* GetInstance() {
    return base::Singleton<ArcTtsServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcTtsServiceFactory>;
  ArcTtsServiceFactory() = default;
  ~ArcTtsServiceFactory() override = default;
};

}  // namespace

// static
ArcTtsService* ArcTtsService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcTtsServiceFactory::GetForBrowserContext(context);
}

ArcTtsService::ArcTtsService(content::BrowserContext* context,
                             ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service), binding_(this) {
  arc_bridge_service_->tts()->AddObserver(this);
}

ArcTtsService::~ArcTtsService() {
  // TODO(hidehiko): Currently, the lifetime of ArcBridgeService and
  // BrowserContextKeyedService is not nested.
  // If ArcServiceManager::Get() returns nullptr, it is already destructed,
  // so do not touch it.
  if (ArcServiceManager::Get())
    arc_bridge_service_->tts()->RemoveObserver(this);
}

void ArcTtsService::OnInstanceReady() {
  mojom::TtsInstance* tts_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->tts(), Init);
  DCHECK(tts_instance);
  mojom::TtsHostPtr host_proxy;
  binding_.Bind(mojo::MakeRequest(&host_proxy));
  tts_instance->Init(std::move(host_proxy));
}

void ArcTtsService::OnTtsEvent(uint32_t id,
                               mojom::TtsEventType event_type,
                               uint32_t char_index,
                               const std::string& error_msg) {
  if (!TtsController::GetInstance())
    return;

  TtsEventType chrome_event_type;
  switch (event_type) {
    case mojom::TtsEventType::START:
      chrome_event_type = TTS_EVENT_START;
      break;
    case mojom::TtsEventType::END:
      chrome_event_type = TTS_EVENT_END;
      break;
    case mojom::TtsEventType::INTERRUPTED:
      chrome_event_type = TTS_EVENT_INTERRUPTED;
      break;
    case mojom::TtsEventType::ERROR:
      chrome_event_type = TTS_EVENT_ERROR;
      break;
  }
  TtsController::GetInstance()->OnTtsEvent(id, chrome_event_type, char_index,
                                           error_msg);
}

}  // namespace arc
