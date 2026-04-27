#include "linear_function_accelerator.hh"

#include <cstddef>
#include <cstring>

#include "base/logging.hh"
#include "dev/arm/base_gic.hh"
#include "mem/packet.hh"
#include "mem/packet_access.hh"
#include "sim/core.hh"

namespace gem5 {

namespace {

int32_t wrapLinearFunction(int32_t a, int32_t x, int32_t b) {
    const int64_t result = static_cast<int64_t>(a) * static_cast<int64_t>(x) +
                           static_cast<int64_t>(b);

    return static_cast<int32_t>(static_cast<uint32_t>(result));
}

} // anonymous namespace

LinearFunctionAccelerator::LinearFunctionAccelerator(const Params &p)
    : BasicPioDevice(p, p.pio_size), controlReg(0), irqClearReg(0),
      irqStatus(IrqStatus::ReadyToProcess), pendingInputVector{0, 0, 0, 0},
      activeInputVector{0, 0, 0, 0}, queuedInputVector{0, 0, 0, 0},
      outputVector{0, 0, 0, 0}, pendingInputByteMask(0), hasQueuedInput(false),
      _interrupt(p.interrupt->get()),
      processEvent([this] { processVector(); }, name() + ".processEvent") {}

int32_t LinearFunctionAccelerator::coefficientA() const {
    ControlRegister reg(controlReg);
    return static_cast<int32_t>(reg.a_bits);
}

int32_t LinearFunctionAccelerator::coefficientB() const {
    ControlRegister reg(controlReg);
    return static_cast<int32_t>(reg.b_bits);
}

void LinearFunctionAccelerator::updateControlRegister(Addr off,
                                                      size_t accessSize,
                                                      uint64_t value) {
    if (off == controlRegOffset && accessSize == controlRegSize) {
        controlReg = value;
        return;
    }

    if (accessSize != sizeof(uint32_t)) {
        panic(
            "LinearFunctionAccelerator: invalid control write addr=%#x size=%u",
            pioAddr + off, accessSize);
    }

    if (off == controlRegOffset) {
        controlReg &= 0xffffffff00000000ULL;
        controlReg |= static_cast<uint32_t>(value);
        return;
    }

    if (off == controlRegOffset + sizeof(uint32_t)) {
        controlReg &= 0x00000000ffffffffULL;
        controlReg |= static_cast<uint64_t>(static_cast<uint32_t>(value)) << 32;
        return;
    }

    panic("LinearFunctionAccelerator: invalid control write addr=%#x size=%u",
          pioAddr + off, accessSize);
}

void LinearFunctionAccelerator::scheduleVectorProcessing(
    const std::array<int32_t, vectorWidth> &inputVector) {
    panic_if(processEvent.scheduled(),
             "LinearFunctionAccelerator: processing event already scheduled");
    activeInputVector = inputVector;
    schedule(processEvent, curTick() + 50 * sim_clock::as_int::ns);
}

void LinearFunctionAccelerator::processVector() {
    if (irqStatus != IrqStatus::ReadyToProcess) {
        schedule(processEvent, curTick() + 10 * sim_clock::as_int::ns);
        return;
    }

    const int32_t a = coefficientA();
    const int32_t b = coefficientB();

    for (size_t lane = 0; lane < vectorWidth; ++lane) {
        outputVector[lane] = wrapLinearFunction(a, activeInputVector[lane], b);
    }

    inform("LinearFunctionAccelerator: a=%d b=%d in=[%d, %d, %d, %d] out=[%d, "
           "%d, %d, %d]",
           a, b, activeInputVector[0], activeInputVector[1],
           activeInputVector[2], activeInputVector[3], outputVector[0],
           outputVector[1], outputVector[2], outputVector[3]);

    irqStatus = IrqStatus::WaitForOutputRead;
    _interrupt->raise();
}

Tick LinearFunctionAccelerator::read(PacketPtr pkt) {
    const Addr off = pkt->getAddr() - pioAddr;
    const size_t size = pkt->getSize();
    pkt->makeAtomicResponse();

    if (off == controlRegOffset && size == controlRegSize) {
        pkt->setLE<uint64_t>(controlReg);
        return pioDelay;
    }

    if (off == irqClearRegOffset && size == sizeof(uint64_t)) {
        pkt->setLE<uint64_t>(irqClearReg);
        return pioDelay;
    }

    if ((off == controlRegOffset ||
         off == controlRegOffset + sizeof(uint32_t)) &&
        size == sizeof(uint32_t)) {
        const uint32_t word =
            static_cast<uint32_t>(controlReg >> ((off - controlRegOffset) * 8));
        pkt->setLE<uint32_t>(word);
        return pioDelay;
    }

    if (off >= outputVectorOffset && off < outputVectorOffset + vectorSize) {
        const Addr outputEnd = outputVectorOffset + vectorSize;
        const Addr readEnd = off + size;

        if (size == 0 || readEnd > outputEnd) {
            panic("LinearFunctionAccelerator: invalid output read addr=%#x "
                  "size=%u",
                  pkt->getAddr(), size);
        }

        const auto *outputBytes =
            reinterpret_cast<const uint8_t *>(outputVector.data());
        const size_t byteOffset = off - outputVectorOffset;
        std::memcpy(pkt->getPtr<uint8_t>(), outputBytes + byteOffset, size);

        return pioDelay;
    }

    panic("LinearFunctionAccelerator: invalid read addr=%#x size=%u",
          pkt->getAddr(), size);
}

Tick LinearFunctionAccelerator::write(PacketPtr pkt) {
    const Addr off = pkt->getAddr() - pioAddr;
    const size_t size = pkt->getSize();
    pkt->makeAtomicResponse();

    if (off == irqClearRegOffset) {
        if (size != sizeof(uint64_t)) {
            panic("LinearFunctionAccelerator: invalid irq clear write addr=%#x "
                  "size=%u",
                  pkt->getAddr(), size);
        }

        irqClearReg = pkt->getLE<uint64_t>();
        if (irqClearReg & 0x1ULL) {
            inform("LFA: Clear interrupt.");
            _interrupt->clear();
            irqStatus = IrqStatus::ReadyToProcess;
            if (hasQueuedInput) {
                hasQueuedInput = false;
                scheduleVectorProcessing(queuedInputVector);
            }
        }
        return pioDelay;
    }

    if (off == controlRegOffset || off == controlRegOffset + sizeof(uint32_t)) {
        if (size != controlRegSize && size != sizeof(uint32_t)) {
            panic("LinearFunctionAccelerator: invalid control write addr=%#x "
                  "size=%u",
                  pkt->getAddr(), size);
        }

        updateControlRegister(off, size,
                              size == controlRegSize ? pkt->getLE<uint64_t>()
                                                     : pkt->getLE<uint32_t>());
        inform("LinearFunctionAccelerator: control <- %#llx (a=%d, b=%d)",
               static_cast<unsigned long long>(controlReg), coefficientA(),
               coefficientB());
        return pioDelay;
    }

    if (off >= inputVectorOffset && off < inputVectorOffset + vectorSize) {
        const Addr inputStart = inputVectorOffset;
        const Addr inputEnd = inputVectorOffset + vectorSize;
        const Addr writeEnd = off + size;

        if (size == 0 || writeEnd > inputEnd) {
            panic("LinearFunctionAccelerator: invalid input write addr=%#x "
                  "size=%u",
                  pkt->getAddr(), size);
        }

        auto *inputBytes =
            reinterpret_cast<uint8_t *>(pendingInputVector.data());
        const size_t byteOffset = off - inputStart;
        std::memcpy(inputBytes + byteOffset, pkt->getPtr<uint8_t>(), size);

        for (size_t i = 0; i < size; ++i) {
            pendingInputByteMask |=
                static_cast<uint16_t>(1U << (byteOffset + i));
        }

        constexpr uint16_t allInputBytesWritten =
            static_cast<uint16_t>((1U << vectorSize) - 1U);
        if (pendingInputByteMask == allInputBytesWritten) {
            pendingInputByteMask = 0;
            const bool busy = irqStatus == IrqStatus::WaitForOutputRead ||
                              processEvent.scheduled();
            if (busy) {
                panic_if(hasQueuedInput,
                         "LinearFunctionAccelerator: only one queued input "
                         "vector is supported");
                queuedInputVector = pendingInputVector;
                hasQueuedInput = true;
            } else {
                scheduleVectorProcessing(pendingInputVector);
            }
        }
        return pioDelay;
    }

    panic("LinearFunctionAccelerator: invalid write addr=%#x size=%u",
          pkt->getAddr(), size);
}

} // namespace gem5
