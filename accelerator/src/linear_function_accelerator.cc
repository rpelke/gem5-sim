#include "linear_function_accelerator.hh"

#include <cstddef>
#include <cstring>

#include "base/logging.hh"
#include "mem/packet.hh"
#include "mem/packet_access.hh"

namespace gem5 {

namespace {

int32_t wrapLinearFunction(int32_t a, int32_t x, int32_t b) {
    const int64_t result = static_cast<int64_t>(a) * static_cast<int64_t>(x) +
                           static_cast<int64_t>(b);

    return static_cast<int32_t>(static_cast<uint32_t>(result));
}

} // anonymous namespace

LinearFunctionAccelerator::LinearFunctionAccelerator(const Params &p)
    : BasicPioDevice(p, p.pio_size), controlReg(0),
      pendingInputVector{0, 0, 0, 0}, outputVector{0, 0, 0, 0},
      pendingLaneMask(0) {}

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

void LinearFunctionAccelerator::processVector(
    const std::array<int32_t, vectorWidth> &inputVector) {
    const int32_t a = coefficientA();
    const int32_t b = coefficientB();

    for (size_t lane = 0; lane < vectorWidth; ++lane) {
        outputVector[lane] = wrapLinearFunction(a, inputVector[lane], b);
    }

    inform("LinearFunctionAccelerator: a=%d b=%d in=[%d, %d, %d, %d] out=[%d, "
           "%d, %d, %d]",
           a, b, inputVector[0], inputVector[1], inputVector[2], inputVector[3],
           outputVector[0], outputVector[1], outputVector[2], outputVector[3]);
}

Tick LinearFunctionAccelerator::read(PacketPtr pkt) {
    const Addr off = pkt->getAddr() - pioAddr;
    const size_t size = pkt->getSize();
    pkt->makeAtomicResponse();

    if (off == controlRegOffset && size == controlRegSize) {
        pkt->setLE<uint64_t>(controlReg);
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

    if (off == outputVectorOffset && size == vectorSize) {
        std::memcpy(pkt->getPtr<uint8_t>(), outputVector.data(), vectorSize);
        return pioDelay;
    }

    if (off >= outputVectorOffset && off < outputVectorOffset + vectorSize &&
        size == sizeof(int32_t) &&
        ((off - outputVectorOffset) % sizeof(int32_t) == 0)) {
        const size_t lane = (off - outputVectorOffset) / sizeof(int32_t);
        pkt->setLE<int32_t>(outputVector[lane]);
        return pioDelay;
    }

    panic("LinearFunctionAccelerator: invalid read addr=%#x size=%u",
          pkt->getAddr(), size);
}

Tick LinearFunctionAccelerator::write(PacketPtr pkt) {
    const Addr off = pkt->getAddr() - pioAddr;
    const size_t size = pkt->getSize();
    pkt->makeAtomicResponse();

    if (off == controlRegOffset || off == controlRegOffset + sizeof(uint32_t)) {
        if (size != controlRegSize && size != sizeof(uint32_t)) {
            panic("LinearFunctionAccelerator: invalid control write addr=%#x "
                  "size=%u",
                  pkt->getAddr(), size);
        }

        updateControlRegister(off, size,
                              size == controlRegSize ? pkt->getLE<uint64_t>()
                                                     : pkt->getLE<uint32_t>());
        inform(
            "LinearFunctionAccelerator: control <- %#llx (a=%d, b=%d)",
            static_cast<unsigned long long>(controlReg),
            coefficientA(), coefficientB());
        return pioDelay;
    }

    if (off == inputVectorOffset && size == vectorSize) {
        std::memcpy(
            pendingInputVector.data(), pkt->getPtr<uint8_t>(), vectorSize);
        pendingLaneMask = 0;
        processVector(pendingInputVector);
        return pioDelay;
    }

    if (off >= inputVectorOffset && off < inputVectorOffset + vectorSize &&
        size == sizeof(int32_t) &&
        ((off - inputVectorOffset) % sizeof(int32_t) == 0)) {
        const size_t lane = (off - inputVectorOffset) / sizeof(int32_t);
        pendingInputVector[lane] = pkt->getLE<int32_t>();
        pendingLaneMask |= static_cast<uint8_t>(1U << lane);
        if (pendingLaneMask == 0x0f) {
            pendingLaneMask = 0;
            processVector(pendingInputVector);
        }
        return pioDelay;
    }

    panic("LinearFunctionAccelerator: invalid write addr=%#x size=%u",
          pkt->getAddr(), size);
}

} // namespace gem5
