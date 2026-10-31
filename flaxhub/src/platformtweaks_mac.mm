#include "platformtweaks.h"

#import <Foundation/Foundation.h>

void PlatformTweaks::disableFullScreenMenuItem()
{
    [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"NSFullScreenMenuItemEverywhere"];
}
