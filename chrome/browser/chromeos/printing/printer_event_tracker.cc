// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printer_event_tracker.h"

namespace chromeos {
namespace {

// Set the event_type on |event| based on |mode|.
void SetEventType(metrics::PrinterEventProto* event,
                  PrinterEventTracker::SetupMode mode) {
  switch (mode) {
    case PrinterEventTracker::kUnknownMode:
      event->set_event_type(metrics::PrinterEventProto::UNKNOWN);
      break;
    case PrinterEventTracker::kUser:
      event->set_event_type(metrics::PrinterEventProto::SETUP_MANUAL);
      break;
    case PrinterEventTracker::kAutomatic:
      event->set_event_type(metrics::PrinterEventProto::SETUP_AUTOMATIC);
      break;
  }
}

// Populate PPD information in |event| based on |ppd|.
void SetPpdInfo(metrics::PrinterEventProto* event,
                const Printer::PpdReference& ppd) {
  if (!ppd.user_supplied_ppd_url.empty()) {
    event->set_user_ppd(true);
  } else if (!ppd.effective_make_and_model.empty()) {
    event->set_ppd_identifier(ppd.effective_make_and_model);
  }
  // PPD information is not populated for autoconfigured printers.
}

// Add information to |event| specific to |usb_printer|.
void SetUsbInfo(metrics::PrinterEventProto* event,
                const PrinterDetector::DetectedPrinter& detected) {
  event->set_usb_vendor_id(detected.ppd_search_data.usb_vendor_id);
  event->set_usb_model_id(detected.ppd_search_data.usb_product_id);

  // TODO(skau) - Pull USB-specific manufacturer/model strings directly into
  // DetectedPrinter and supply those here instead of the deprecated Printer
  // fields.
  event->set_usb_printer_manufacturer(detected.printer.manufacturer());
  event->set_usb_printer_model(detected.printer.model());
}

// Add information to the |event| that only network printers have.
void SetNetworkPrinterInfo(metrics::PrinterEventProto* event,
                           const Printer& printer) {
  if (!printer.make_and_model().empty()) {
    event->set_ipp_make_and_model(printer.make_and_model());
  }
}

}  // namespace

PrinterEventTracker::PrinterEventTracker() = default;
PrinterEventTracker::~PrinterEventTracker() = default;

void PrinterEventTracker::set_logging(bool logging) {
  logging_ = logging;
}

void PrinterEventTracker::RecordUsbPrinterInstalled(
    const PrinterDetector::DetectedPrinter& detected,
    SetupMode mode) {
  if (!logging_) {
    return;
  }

  metrics::PrinterEventProto event;
  SetEventType(&event, mode);
  SetPpdInfo(&event, detected.printer.ppd_reference());
  SetUsbInfo(&event, detected);
  events_.push_back(event);
}

void PrinterEventTracker::RecordIppPrinterInstalled(const Printer& printer,
                                                    SetupMode mode) {
  if (!logging_) {
    return;
  }

  metrics::PrinterEventProto event;
  SetEventType(&event, mode);
  SetPpdInfo(&event, printer.ppd_reference());
  SetNetworkPrinterInfo(&event, printer);
  events_.push_back(event);
}

void PrinterEventTracker::RecordUsbSetupAbandoned(
    const PrinterDetector::DetectedPrinter& detected) {
  if (!logging_) {
    return;
  }

  metrics::PrinterEventProto event;
  event.set_event_type(metrics::PrinterEventProto::SETUP_ABANDONED);
  SetUsbInfo(&event, detected);
  events_.push_back(event);
}

void PrinterEventTracker::RecordSetupAbandoned(const Printer& printer) {
  if (!logging_) {
    return;
  }

  metrics::PrinterEventProto event;
  event.set_event_type(metrics::PrinterEventProto::SETUP_ABANDONED);
  SetNetworkPrinterInfo(&event, printer);
  events_.push_back(event);
}

void PrinterEventTracker::RecordPrinterRemoved(const Printer& printer) {
  if (!logging_) {
    return;
  }

  metrics::PrinterEventProto event;
  event.set_event_type(metrics::PrinterEventProto::PRINTER_DELETED);
  SetNetworkPrinterInfo(&event, printer);
  SetPpdInfo(&event, printer.ppd_reference());
  events_.push_back(event);
}

void PrinterEventTracker::FlushPrinterEvents(
    std::vector<metrics::PrinterEventProto>* events) {
  events->swap(events_);
  events_.clear();
}

}  // namespace chromeos
