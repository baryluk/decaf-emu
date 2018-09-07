#pragma once
#include "gpu_graphicsdriver.h"

namespace vulkan
{

class Driver : public gpu::GraphicsDriver
{
public:
   virtual ~Driver() = default;
   virtual gpu::GraphicsDriverType type() override;

   virtual void run() override;
   virtual void stop() override;
   virtual void runUntilFlip() override;
   
   virtual float getAverageFPS() override;
   virtual float getAverageFrametimeMS() override;

   virtual void notifyCpuFlush(phys_addr address, uint32_t size) override;
   virtual void notifyGpuFlush(phys_addr address, uint32_t size) override;

private:
   bool mRunning = false;
};

} // namespace vulkan
