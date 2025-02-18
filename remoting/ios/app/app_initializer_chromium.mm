// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/app_initializer.h"

#import "remoting/ios/app/help_and_feedback.h"
#import "remoting/ios/facade/remoting_oauth_authentication.h"
#import "remoting/ios/facade/remoting_service.h"

@implementation AppInitializer

+ (void)initializeApp {
  // |authentication| is nil by default and needs to be injected here.
  RemotingService.instance.authentication =
      [[RemotingOAuthAuthentication alloc] init];
  HelpAndFeedback.instance = [[HelpAndFeedback alloc] init];
}

@end
