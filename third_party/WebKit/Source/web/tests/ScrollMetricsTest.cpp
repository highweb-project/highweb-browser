// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/LocalFrameView.h"
#include "core/frame/WebLocalFrameBase.h"
#include "core/geometry/DOMRect.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "core/testing/sim/SimDisplayItemList.h"
#include "core/testing/sim/SimRequest.h"
#include "core/testing/sim/SimTest.h"
#include "platform/scroll/MainThreadScrollingReason.h"
#include "platform/scroll/ScrollerSizeMetrics.h"
#include "platform/testing/HistogramTester.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/UnitTestHelpers.h"

#define EXPECT_WHEEL_BUCKET(reason, count)     \
  histogram_tester.ExpectBucketCount(          \
      "Renderer4.MainThreadWheelScrollReason", \
      GetBucketIndex(MainThreadScrollingReason::reason), count);

#define EXPECT_TOUCH_BUCKET(reason, count)       \
  histogram_tester.ExpectBucketCount(            \
      "Renderer4.MainThreadGestureScrollReason", \
      GetBucketIndex(MainThreadScrollingReason::reason), count);

#define EXPECT_WHEEL_TOTAL(count)                                            \
  histogram_tester.ExpectTotalCount("Renderer4.MainThreadWheelScrollReason", \
                                    count);

#define EXPECT_TOUCH_TOTAL(count)                                              \
  histogram_tester.ExpectTotalCount("Renderer4.MainThreadGestureScrollReason", \
                                    count);

namespace blink {

namespace {

class ScrollMetricsTest : public SimTest {
 public:
  void SetUpHtml(const char*);
  void Scroll(Element*, const WebGestureDevice);
};

class NonCompositedMainThreadScrollingReasonRecordTest
    : public ScrollMetricsTest {
 protected:
  int GetBucketIndex(uint32_t reason);
};

class ScrollBeginEventBuilder : public WebGestureEvent {
 public:
  ScrollBeginEventBuilder(IntPoint position,
                          FloatPoint delta,
                          WebGestureDevice device)
      : WebGestureEvent() {
    type_ = WebInputEvent::kGestureScrollBegin;
    x = global_x = position.X();
    y = global_y = position.Y();
    data.scroll_begin.delta_y_hint = delta.Y();
    source_device = device;
    frame_scale_ = 1;
  }
};

class ScrollUpdateEventBuilder : public WebGestureEvent {
 public:
  ScrollUpdateEventBuilder() : WebGestureEvent() {
    type_ = WebInputEvent::kGestureScrollUpdate;
    data.scroll_update.delta_x = 0.0f;
    data.scroll_update.delta_y = 1.0f;
    data.scroll_update.velocity_x = 0;
    data.scroll_update.velocity_y = 1;
    frame_scale_ = 1;
  }
};

class ScrollEndEventBuilder : public WebGestureEvent {
 public:
  ScrollEndEventBuilder() : WebGestureEvent() {
    type_ = WebInputEvent::kGestureScrollEnd;
    frame_scale_ = 1;
  }
};

int NonCompositedMainThreadScrollingReasonRecordTest::GetBucketIndex(
    uint32_t reason) {
  int index = 1;
  while (!(reason & 1)) {
    reason >>= 1;
    ++index;
  }
  DCHECK_EQ(reason, 1u);
  return index;
}

void ScrollMetricsTest::Scroll(Element* element,
                               const WebGestureDevice device) {
  DCHECK(element);
  DCHECK(element->getBoundingClientRect());
  DOMRect* rect = element->getBoundingClientRect();
  ScrollBeginEventBuilder scroll_begin(
      IntPoint(rect->left() + rect->width() / 2,
               rect->top() + rect->height() / 2),
      FloatPoint(0.f, 1.f), device);
  ScrollUpdateEventBuilder scroll_update;
  ScrollEndEventBuilder scroll_end;
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(scroll_begin);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(scroll_update);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(scroll_end);
  ASSERT_GT(scroll_update.DeltaYInRootFrame(), 0);
}

void ScrollMetricsTest::SetUpHtml(const char* html_content) {
  WebView().Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(html_content);
  Compositor().BeginFrame();
}

TEST_F(ScrollMetricsTest, ScrollerSizeOnPageLoadHistogramRecordingTest) {
  HistogramTester histogram_tester;
  SetUpHtml(
      "<!DOCTYPE html>"
      "<style>"
      " .smallBox { overflow: scroll; width: 100px; height: 100px; }"
      " .largeBox { overflow: auto; width: 500px; height: 500px; }"
      " .nonScrollableBox { overflow: auto; width: 200px; height: 200px; }"
      " .spacer { height: 1000px; }"
      " body { height: 2000px; width: 1000px; }"
      "</style>"
      "<div class='smallBox'><div class='spacer'></div></div>"
      "<div class='largeBox'><div class='spacer'></div></div>"
      "<div class='notScrollableBox'></div>"
      "<div style='width: 300px; height: 300px;'>"
      " <div class='spacer'></div>"
      "</div>");

  testing::RunPendingTasks();

  // SmallBox added to count.
  histogram_tester.ExpectBucketCount("Event.Scroll.ScrollerSize.OnLoad", 10000,
                                     1);
  // LargeBox added to count.
  histogram_tester.ExpectBucketCount("Event.Scroll.ScrollerSize.OnLoad",
                                     kScrollerSizeLargestBucket, 1);

  // Non-scrollable box is not added to count.
  histogram_tester.ExpectBucketCount("Event.Scroll.ScrollerSize.OnLoad", 40000,
                                     0);

  // The fourth div without "overflow: scroll" is not supposed to be added to
  // count.
  histogram_tester.ExpectBucketCount("Event.Scroll.ScrollerSize.OnLoad", 90000,
                                     0);
  // Root scroller is not added to count.
  histogram_tester.ExpectBucketCount("Event.Scroll.ScrollerSize.OnLoad",
                                     kScrollerSizeLargestBucket, 1);
  histogram_tester.ExpectTotalCount("Event.Scroll.ScrollerSize.OnLoad", 2);
}

TEST_F(ScrollMetricsTest,
       ScrollerSizeOfMainThreadScrollingHistogramRecordingTest) {
  HistogramTester histogram_tester;
  SetUpHtml(
      "<!DOCTYPE html>"
      "<style>"
      " .container { width: 200px; height: 200px; overflow: scroll; }"
      " .box { width: 100px; height: 100px; overflow: scroll; }"
      " .spacer { height: 1000px; }"
      " .target { width: 200px; height: 200px; }"
      " body { height: 2000px; }"
      "</style>"
      "<div class='container'>"
      " <div id='box' class='box'>"
      "  <div id='content' class='spacer'></div>"
      " </div>"
      "</div>"
      "<div id='target' class='target'></div>");

  Element* box = GetDocument().getElementById("box");

  // Test wheel scroll on the box.
  Scroll(box, kWebGestureDeviceTouchpad);
  histogram_tester.ExpectBucketCount("Event.Scroll.ScrollerSize.OnScroll_Wheel",
                                     10000, 1);
  // Only the first scrollable area is recorded.
  histogram_tester.ExpectBucketCount("Event.Scroll.ScrollerSize.OnScroll_Wheel",
                                     40000, 0);
  histogram_tester.ExpectTotalCount("Event.Scroll.ScrollerSize.OnScroll_Wheel",
                                    1);

  // Test touch scroll.
  Scroll(box, kWebGestureDeviceTouchscreen);
  histogram_tester.ExpectBucketCount("Event.Scroll.ScrollerSize.OnScroll_Touch",
                                     10000, 1);
  histogram_tester.ExpectTotalCount("Event.Scroll.ScrollerSize.OnScroll_Touch",
                                    1);

  // Scrolling the non-scrollable target leads to scroll the root layer which
  // doesn't add to count.
  Element* body_scroll_target = GetDocument().getElementById("target");
  Scroll(body_scroll_target, kWebGestureDeviceTouchscreen);
  histogram_tester.ExpectBucketCount("Event.Scroll.ScrollerSize.OnScroll_Touch",
                                     kScrollerSizeLargestBucket, 0);
  histogram_tester.ExpectTotalCount("Event.Scroll.ScrollerSize.OnScroll_Touch",
                                    1);
}

TEST_F(NonCompositedMainThreadScrollingReasonRecordTest,
       TouchAndWheelGeneralTest) {
  SetUpHtml(
      "<style>"
      " .box { overflow:scroll; width: 100px; height: 100px; }"
      " .translucent { opacity: 0.5; }"
      " .spacer { height: 1000px; }"
      "</style>"
      "<div id='box' class='translucent box'>"
      " <div class='spacer'></div>"
      "</div>");

  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* box = GetDocument().getElementById("box");
  HistogramTester histogram_tester;

  // Test touch scroll.
  Scroll(box, kWebGestureDeviceTouchscreen);
  EXPECT_TOUCH_BUCKET(kHasOpacityAndLCDText, 1);
  EXPECT_TOUCH_BUCKET(kBackgroundNotOpaqueInRectAndLCDText, 1);

  Scroll(box, kWebGestureDeviceTouchscreen);
  EXPECT_TOUCH_BUCKET(kHasOpacityAndLCDText, 2);
  EXPECT_TOUCH_BUCKET(kBackgroundNotOpaqueInRectAndLCDText, 2);
  EXPECT_TOUCH_TOTAL(4);

  // Test wheel scroll.
  Scroll(box, kWebGestureDeviceTouchpad);
  EXPECT_WHEEL_BUCKET(kHasOpacityAndLCDText, 1);
  EXPECT_WHEEL_BUCKET(kBackgroundNotOpaqueInRectAndLCDText, 1);
  EXPECT_WHEEL_TOTAL(2);
}

TEST_F(NonCompositedMainThreadScrollingReasonRecordTest,
       CompositedScrollableAreaTest) {
  SetUpHtml(
      "<style>"
      " .box { overflow:scroll; width: 100px; height: 100px; }"
      " .translucent { opacity: 0.5; }"
      " .composited { will-change: transform; }"
      " .spacer { height: 1000px; }"
      "</style>"
      "<div id='box' class='translucent box'>"
      " <div class='spacer'></div>"
      "</div>");

  WebView().GetSettings()->SetAcceleratedCompositingEnabled(true);
  GetDocument().View()->SetParentVisible(true);
  GetDocument().View()->SetSelfVisible(true);
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* box = GetDocument().getElementById("box");
  HistogramTester histogram_tester;

  Scroll(box, kWebGestureDeviceTouchpad);
  EXPECT_WHEEL_BUCKET(kHasOpacityAndLCDText, 1);
  EXPECT_WHEEL_BUCKET(kBackgroundNotOpaqueInRectAndLCDText, 1);
  EXPECT_WHEEL_TOTAL(2);

  box->setAttribute("class", "composited translucent box");
  GetDocument().View()->UpdateAllLifecyclePhases();
  Scroll(box, kWebGestureDeviceTouchpad);
  EXPECT_FALSE(ToLayoutBox(box->GetLayoutObject())
                   ->GetScrollableArea()
                   ->GetNonCompositedMainThreadScrollingReasons());
  EXPECT_WHEEL_BUCKET(kHasOpacityAndLCDText, 1);
  EXPECT_WHEEL_BUCKET(kBackgroundNotOpaqueInRectAndLCDText, 1);
  EXPECT_WHEEL_TOTAL(2);
}

TEST_F(NonCompositedMainThreadScrollingReasonRecordTest,
       NotScrollableAreaTest) {
  SetUpHtml(
      "<style>.box { overflow:scroll; width: 100px; height: 100px; }"
      " .translucent { opacity: 0.5; }"
      " .hidden { overflow: hidden; }"
      " .spacer { height: 1000px; }"
      "</style>"
      "<div id='box' class='translucent box'>"
      " <div class='spacer'></div>"
      "</div>");

  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* box = GetDocument().getElementById("box");
  HistogramTester histogram_tester;

  Scroll(box, kWebGestureDeviceTouchpad);
  EXPECT_WHEEL_BUCKET(kHasOpacityAndLCDText, 1);
  EXPECT_WHEEL_BUCKET(kBackgroundNotOpaqueInRectAndLCDText, 1);
  EXPECT_WHEEL_TOTAL(2);

  box->setAttribute("class", "hidden translucent box");
  GetDocument().View()->UpdateAllLifecyclePhases();
  Scroll(box, kWebGestureDeviceTouchpad);
  EXPECT_WHEEL_BUCKET(kHasOpacityAndLCDText, 1);
  EXPECT_WHEEL_BUCKET(kBackgroundNotOpaqueInRectAndLCDText, 1);
  EXPECT_WHEEL_TOTAL(2);
}

TEST_F(NonCompositedMainThreadScrollingReasonRecordTest, NestedScrollersTest) {
  SetUpHtml(
      "<style>"
      " .container { overflow:scroll; width: 200px; height: 200px; }"
      " .box { overflow:scroll; width: 100px; height: 100px; }"
      " .translucent { opacity: 0.5; }"
      " .transform { transform: scale(0.8); }"
      " .with-border-radius { border: 5px solid; border-radius: 5px; }"
      " .spacer { height: 1000px; }"
      " .composited { will-change: transform; }"
      "</style>"
      "<div id='container' class='container with-border-radius'>"
      "  <div class='translucent box'>"
      "    <div id='inner' class='composited transform box'>"
      "      <div class='spacer'></div>"
      "    </div>"
      "    <div class='spacer'></div>"
      "  </div>"
      "  <div class='spacer'></div>"
      "</div>");

  WebView().GetSettings()->SetAcceleratedCompositingEnabled(true);
  GetDocument().View()->SetParentVisible(true);
  GetDocument().View()->SetSelfVisible(true);
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* box = GetDocument().getElementById("inner");
  HistogramTester histogram_tester;

  Scroll(box, kWebGestureDeviceTouchpad);
  // Scrolling the inner box will gather reasons from the scrolling chain. The
  // inner box itself has no reason because it's composited. Other scrollable
  // areas from the chain have corresponding reasons.
  EXPECT_WHEEL_BUCKET(kHasOpacityAndLCDText, 1);
  EXPECT_WHEEL_BUCKET(kBackgroundNotOpaqueInRectAndLCDText, 1);
  EXPECT_WHEEL_BUCKET(kIsNotStackingContextAndLCDText, 1);
  EXPECT_WHEEL_BUCKET(kHasBorderRadius, 1);
  EXPECT_WHEEL_BUCKET(kHasTransformAndLCDText, 0);
  EXPECT_WHEEL_TOTAL(4);
}

}  // namespace

}  // namespace blink
