/*
 *  SPDX-FileCopyrightText: 2021 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KoOptimizedPixelDataScalerU8ToU16Factory.h"
#include "KoOptimizedPixelDataScalerU8ToU16FactoryImpl.h"
#include <iostream>

KoOptimizedPixelDataScalerU8ToU16Base *KoOptimizedPixelDataScalerU8ToU16Factory::createRgbaScaler()
{
#ifdef KRITA_GUI
    return createOptimizedClass<KoOptimizedPixelDataScalerU8ToU16FactoryImpl>(4);
#else
    std::cout << "KRITA HEADLESS - TODO: KoOptimizedPixelDataScalerU8ToU16Base" << std::endl;
    return nullptr;
#endif
}

KoOptimizedPixelDataScalerU8ToU16Base *KoOptimizedPixelDataScalerU8ToU16Factory::createCmykaScaler()
{
#ifdef KRITA_GUI
    return createOptimizedClass<KoOptimizedPixelDataScalerU8ToU16FactoryImpl>(5);
#else
    std::cout << "KRITA HEADLESS - TODO: KoOptimizedPixelDataScalerU8ToU16Base" << std::endl;
    return nullptr;
#endif
}
