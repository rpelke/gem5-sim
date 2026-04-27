#ifndef __LINEAR_FUNCTION_ACCELERATOR_HH__
#define __LINEAR_FUNCTION_ACCELERATOR_HH__

#include <array>
#include <cstdint>

#include "base/bitunion.hh"
#include "dev/io_device.hh"
#include "params/LinearFunctionAccelerator.hh"
#include "sim/eventq.hh"

namespace gem5 {

class ArmInterruptPin;

BitUnion64(ControlRegister) Bitfield<31, 0> a_bits;
Bitfield<63, 32> b_bits;
EndBitUnion(ControlRegister)

class LinearFunctionAccelerator : public BasicPioDevice {
  private:
    enum class IrqStatus : uint8_t {
        ReadyToProcess = 0,
        WaitForOutputRead = 1
    };

    static constexpr Addr controlRegOffset = 0x00;
    static constexpr Addr inputVectorOffset = 0x10;
    static constexpr Addr outputVectorOffset = 0x20;
    static constexpr Addr irqClearRegOffset = 0x30;
    static constexpr size_t vectorWidth = 4;
    static constexpr size_t controlRegSize = sizeof(uint64_t);
    static constexpr size_t vectorSize = vectorWidth * sizeof(int32_t);

    uint64_t controlReg;
    uint64_t irqClearReg;
    IrqStatus irqStatus;
    std::array<int32_t, vectorWidth> pendingInputVector;
    std::array<int32_t, vectorWidth> activeInputVector;
    std::array<int32_t, vectorWidth> queuedInputVector;
    std::array<int32_t, vectorWidth> outputVector;
    uint16_t pendingInputByteMask;
    bool hasQueuedInput;
    ArmInterruptPin *const _interrupt;
    EventFunctionWrapper processEvent;

    int32_t coefficientA() const;
    int32_t coefficientB() const;
    void updateControlRegister(Addr off, size_t accessSize, uint64_t value);
    void scheduleVectorProcessing(
        const std::array<int32_t, vectorWidth> &inputVector);
    void processVector();

  public:
    PARAMS(LinearFunctionAccelerator);
    LinearFunctionAccelerator(const Params &p);

    Tick read(PacketPtr pkt) override;
    Tick write(PacketPtr pkt) override;
};

} // namespace gem5

#endif
