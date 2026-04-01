#include "dummy_mmio.hh"

#include "base/logging.hh"
#include "mem/packet.hh"
#include "mem/packet_access.hh"

namespace gem5
{

DummyMmio::DummyMmio(const Params &p)
    : BasicPioDevice(p, p.pio_size), reg0(0)
{
}

Tick
DummyMmio::read(PacketPtr pkt)
{
    const Addr off = pkt->getAddr() - pioAddr;
    pkt->makeAtomicResponse();

    // Expose the single 32-bit register at offset 0x0.
    if (off == 0x0 && pkt->getSize() == sizeof(uint32_t)) {
        pkt->setLE<uint32_t>(reg0);
    } else {
        panic("DummyMmio: invalid read addr=%#x size=%u",
              pkt->getAddr(), pkt->getSize());
    }

    return pioDelay;
}

Tick
DummyMmio::write(PacketPtr pkt)
{
    const Addr off = pkt->getAddr() - pioAddr;
    pkt->makeAtomicResponse();

    // Store 32-bit writes to the single register and log the new value.
    if (off == 0x0 && pkt->getSize() == sizeof(uint32_t)) {
        reg0 = pkt->getLE<uint32_t>();
        inform("DummyMmio: reg0 <- %#x", reg0);
    } else {
        panic("DummyMmio: invalid write addr=%#x size=%u",
              pkt->getAddr(), pkt->getSize());
    }

    return pioDelay;
}

} // namespace gem5
