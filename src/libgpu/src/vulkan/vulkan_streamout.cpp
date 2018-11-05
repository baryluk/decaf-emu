#ifdef DECAF_VULKAN
#include "vulkan_driver.h"

namespace vulkan
{

StreamContextObject *
Driver::allocateStreamContext(uint32_t initialOffset)
{
   vk::BufferCreateInfo bufferDesc;
   bufferDesc.size = 4;
   bufferDesc.usage =
      vk::BufferUsageFlagBits::eTransformFeedbackBufferEXT |
      vk::BufferUsageFlagBits::eTransferSrc |
      vk::BufferUsageFlagBits::eTransferDst;
   bufferDesc.sharingMode = vk::SharingMode::eExclusive;
   bufferDesc.queueFamilyIndexCount = 0;
   bufferDesc.pQueueFamilyIndices = nullptr;

   VmaAllocationCreateInfo allocInfo = {};
   allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

   VkBuffer buffer;
   VmaAllocation allocation;
   vmaCreateBuffer(mAllocator,
                   &static_cast<VkBufferCreateInfo>(bufferDesc),
                   &allocInfo,
                   &buffer,
                   &allocation,
                   nullptr);

   static uint64_t streamOutCounterIdx = 0;
   setVkObjectName(buffer, fmt::format("strmoutctr_{}", streamOutCounterIdx++).c_str());

   void *mappedPtr;
   vmaMapMemory(mAllocator, allocation, &mappedPtr);
   *reinterpret_cast<uint32_t*>(mappedPtr) = initialOffset;
   vmaUnmapMemory(mAllocator, allocation);

   auto stream = new StreamContextObject();
   stream->allocation = allocation;
   stream->buffer = buffer;
   return stream;
}

void
Driver::releaseStreamContext(StreamContextObject* stream)
{
   vmaDestroyBuffer(mAllocator, stream->buffer, stream->allocation);
}

StreamOutBufferDesc
Driver::getStreamOutBufferDesc(uint32_t bufferIndex)
{
   // If streamout is disabled, then there is no buffer here.
   auto vgt_strmout_en = getRegister<latte::VGT_STRMOUT_EN>(latte::Register::VGT_STRMOUT_EN);
   if (!vgt_strmout_en.STREAMOUT()) {
      return StreamOutBufferDesc();
   }

   StreamOutBufferDesc desc;

   auto vgt_strmout_buffer_base = getRegister<uint32_t>(latte::Register::VGT_STRMOUT_BUFFER_BASE_0 + 16 * bufferIndex);
   auto vgt_strmout_buffer_offset = getRegister<uint32_t>(latte::Register::VGT_STRMOUT_BUFFER_OFFSET_0 + 16 * bufferIndex);
   auto vgt_strmout_buffer_size = getRegister<uint32_t>(latte::Register::VGT_STRMOUT_BUFFER_SIZE_0 + 16 * bufferIndex);
   auto vgt_strmout_vtx_stride = getRegister<uint32_t>(latte::Register::VGT_STRMOUT_VTX_STRIDE_0 + 16 * bufferIndex);

   decaf_check(vgt_strmout_buffer_offset == 0);

   desc.baseAddress = phys_addr(vgt_strmout_buffer_base << 8);
   desc.size = vgt_strmout_buffer_size << 2;
   desc.stride = vgt_strmout_vtx_stride << 2;

   return desc;
}

bool
Driver::checkCurrentStreamOutBuffers()
{
   for (auto i = 0; i < latte::MaxStreamOutBuffers; ++i) {
      auto currentDesc = getStreamOutBufferDesc(i);

      if (!currentDesc.baseAddress) {
         mCurrentStreamOutBuffers[i] = nullptr;
         continue;
      }

      // Fetch the memory cache for this buffer
      auto memCache = getDataMemCache(currentDesc.baseAddress, currentDesc.size);

      // Transition the surface to being a stream out buffer.  This will cause it to be
      // automatically invalidated later on.
      transitionMemCache(memCache, ResourceUsage::StreamOutBuffer);

      mCurrentStreamOutBuffers[i] = memCache;
   }

   return true;
}

void
Driver::bindStreamOutBuffers()
{
   for (auto i = 0; i < latte::MaxStreamOutBuffers; ++i) {
      auto& buffer = mCurrentStreamOutBuffers[i];
      if (!buffer) {
         continue;
      }

      mActiveCommandBuffer.bindTransformFeedbackBuffersEXT(i, { buffer->buffer }, { 0 }, { buffer->size }, mVkDynLoader);
   }
}

void
Driver::beginStreamOut()
{
   std::array<vk::Buffer, latte::MaxStreamOutBuffers> buffers = { vk::Buffer{} };
   std::array<vk::DeviceSize, latte::MaxStreamOutBuffers> offsets = { 0 };
   for (auto i = 0; i < latte::MaxStreamOutBuffers; ++i) {
      auto ctxData = mStreamContextData[i];
      if (ctxData) {
         buffers[i] = ctxData->buffer;
      }
   }

   mActiveCommandBuffer.beginTransformFeedbackEXT(0, buffers, offsets, mVkDynLoader);
}

void
Driver::endStreamOut()
{
   std::array<vk::Buffer, latte::MaxStreamOutBuffers> buffers = { vk::Buffer{} };
   std::array<vk::DeviceSize, latte::MaxStreamOutBuffers> offsets = { 0 };
   for (auto i = 0; i < latte::MaxStreamOutBuffers; ++i) {
      auto ctxData = mStreamContextData[i];
      if (ctxData) {
         buffers[i] = ctxData->buffer;
      }
   }

   mActiveCommandBuffer.endTransformFeedbackEXT(0, buffers, offsets, mVkDynLoader);
}

} // namespace vulkan

#endif // ifdef DECAF_VULKAN
