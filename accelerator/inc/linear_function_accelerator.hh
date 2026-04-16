#ifndef __LINEAR_FUNCTION_ACCELERATOR_HH__
#define __LINEAR_FUNCTION_ACCELERATOR_HH__

#include <array>
#include <cstdint>

#include "base/bitunion.hh"
#include "dev/io_device.hh"
#include "params/LinearFunctionAccelerator.hh"

namespace gem5 {

BitUnion64(ControlRegister) Bitfield<31, 0> a_bits;
Bitfield<63, 32> b_bits;
EndBitUnion(ControlRegister)

    class LinearFunctionAccelerator : public BasicPioDevice {
  private:
    static constexpr Addr controlRegOffset = 0x00;
    static constexpr Addr inputVectorOffset = 0x10;
    static constexpr Addr outputVectorOffset = 0x20;
    static constexpr size_t vectorWidth = 4;
    static constexpr size_t controlRegSize = sizeof(uint64_t);
    static constexpr size_t vectorSize = vectorWidth * sizeof(int32_t);

    uint64_t controlReg;
    std::array<int32_t, vectorWidth> pendingInputVector;
    std::array<int32_t, vectorWidth> outputVector;
    uint16_t pendingInputByteMask;

    int32_t coefficientA() const;
    int32_t coefficientB() const;
    void updateControlRegister(Addr off, size_t accessSize, uint64_t value);
    void processVector(const std::array<int32_t, vectorWidth> &inputVector);

  public:
    PARAMS(LinearFunctionAccelerator);
    LinearFunctionAccelerator(const Params &p);

    Tick read(PacketPtr pkt) override;
    Tick write(PacketPtr pkt) override;
};

} // namespace gem5

#endif
