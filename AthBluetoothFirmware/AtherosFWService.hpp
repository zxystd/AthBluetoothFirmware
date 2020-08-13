//
//  AtherosFWService.hpp
//  Ath3kBT
//
//  Created by qcwap on 2020/8/12.
//  Copyright © 2020 zxystd. All rights reserved.
//

#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>

#include "LinuxTypes.h"

#ifndef AtherosFWService_hpp
#define AtherosFWService_hpp

class AtherosFWService : public IOService {
    OSDeclareDefaultStructors(AtherosFWService)
};

#endif /* AtherosFWService_hpp */
