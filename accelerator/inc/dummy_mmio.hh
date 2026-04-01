#ifndef __MY_DUMMY_MMIO_HH__
#define __MY_DUMMY_MMIO_HH__

#include "dev/io_device.hh"
#include "params/DummyMmio.hh"

namespace gem5
{

class DummyMmio : public BasicPioDevice
{
  private:
    uint32_t reg0;

  public:
    PARAMS(DummyMmio);
    DummyMmio(const Params &p);

    Tick read(PacketPtr pkt) override;
    Tick write(PacketPtr pkt) override;
};

} // namespace gem5

#endif
