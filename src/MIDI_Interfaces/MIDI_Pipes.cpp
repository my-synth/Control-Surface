#include "MIDI_Pipes.hpp"
#include <AH/Error/Error.hpp>
#include <AH/STL/utility>

#if defined(ESP32) || !defined(ARDUINO)
#include <mutex>
#endif

AH_DIAGNOSTIC_WERROR()

BEGIN_CS_NAMESPACE

// :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::: //

void MIDI_Sink::connectSourcePipe(MIDI_Pipe *source) {
    if (this->sourcePipe == nullptr) {
        source->connectSink(this);
        this->sourcePipe = source;
    } else {
        this->sourcePipe->connectSourcePipe(source);
    }
}

void MIDI_Sink::disconnectSourcePipes() {
    if (sourcePipe != nullptr) {
        sourcePipe->disconnectSourcePipes();
        sourcePipe->disconnect();
        sourcePipe = nullptr;
    }
}

void MIDI_Sink::disconnectSourcePipesShallow() {
    if (sourcePipe != nullptr) {
        sourcePipe->disconnectSink();
        sourcePipe = nullptr;
    }
}

bool MIDI_Sink::disconnect(TrueMIDI_Source &source) {
    if (!hasSourcePipe())
        return false;
    return sourcePipe->disconnect(source);
}

MIDI_Sink::~MIDI_Sink() { disconnectSourcePipes(); }

MIDI_Sink::MIDI_Sink(MIDI_Sink &&other)
    : sourcePipe(std::exchange(other.sourcePipe, nullptr)) {
    if (this->hasSourcePipe()) {
        this->sourcePipe->disconnectSink();
        this->sourcePipe->connectSink(this);
    }
}

MIDI_Sink &MIDI_Sink::operator=(MIDI_Sink &&other) {
    std::swap(this->sourcePipe, other.sourcePipe);
    if (this->hasSourcePipe()) {
        this->sourcePipe->disconnectSink();
        this->sourcePipe->connectSink(this);
    }
    if (other.hasSourcePipe()) {
        other.sourcePipe->disconnectSink();
        other.sourcePipe->connectSink(this);
    }
    return *this;
}

// :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::: //

void MIDI_Source::connectSinkPipe(MIDI_Pipe *sink) {
    if (this->sinkPipe == nullptr) {
        sink->connectSource(this);
        this->sinkPipe = sink;
    } else {
        this->sinkPipe->connectSinkPipe(sink);
    }
}

void MIDI_Source::disconnectSinkPipes() {
    if (sinkPipe != nullptr) {
        sinkPipe->disconnectSinkPipes();
        sinkPipe->disconnect();
        sinkPipe = nullptr;
    }
}

void MIDI_Source::disconnectSinkPipesShallow() {
    if (sinkPipe != nullptr) {
        sinkPipe->disconnectSource();
        sinkPipe = nullptr;
    }
}

bool MIDI_Source::disconnect(TrueMIDI_Sink &sink) {
    if (!hasSinkPipe())
        return false;
    return sinkPipe->disconnect(sink);
}

MIDI_Source::MIDI_Source(MIDI_Source &&other)
    : sinkPipe(std::exchange(other.sinkPipe, nullptr)) {
    if (this->hasSinkPipe()) {
        this->sinkPipe->disconnectSource();
        this->sinkPipe->connectSource(this);
    }
}

MIDI_Source &MIDI_Source::operator=(MIDI_Source &&other) {
    std::swap(this->sinkPipe, other.sinkPipe);
    if (this->hasSinkPipe()) {
        this->sinkPipe->disconnectSource();
        this->sinkPipe->connectSource(this);
    }
    if (other.hasSinkPipe()) {
        other.sinkPipe->disconnectSource();
        other.sinkPipe->connectSource(this);
    }
    return *this;
}

MIDI_Source::~MIDI_Source() { disconnectSinkPipes(); }

void MIDI_Source::exclusive(cn_t cn, bool exclusive) {
    if (hasSinkPipe())
        sinkPipe->exclusive(cn, exclusive);
}

bool MIDI_Source::canWrite(cn_t cn) const {
    return !hasSinkPipe() || sinkPipe->isAvailableForWrite(cn);
}

void MIDI_Source::sourceMIDItoPipe(ChannelMessage msg) {
    if (sinkPipe != nullptr) {
        sinkPipe->pipeMIDI(msg);
    }
}
void MIDI_Source::sourceMIDItoPipe(SysExMessage msg) {
    if (sinkPipe != nullptr) {
        sinkPipe->pipeMIDI(msg);
    }
}
void MIDI_Source::sourceMIDItoPipe(RealTimeMessage msg) {
    if (sinkPipe != nullptr) {
        sinkPipe->pipeMIDI(msg);
    }
}

// :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::: //

void MIDI_Pipe::connectSink(MIDI_Sink *sink) {
    if (this->sink != nullptr) {
        FATAL_ERROR(F("This pipe is already connected to a sink"), 0x9145);
        return; // LCOV_EXCL_LINE
    }
    this->sink = sink;
}

void MIDI_Pipe::disconnectSink() { this->sink = nullptr; }

void MIDI_Pipe::connectSource(MIDI_Source *source) {
    if (this->source != nullptr) {
        FATAL_ERROR(F("This pipe is already connected to a source"), 0x9146);
        return; // LCOV_EXCL_LINE
    }
    this->source = source;
}

void MIDI_Pipe::disconnectSource() { this->source = nullptr; }

void MIDI_Pipe::disconnect() {
    if (hasSink() && hasThroughIn()) {
        auto oldSink = sink;
        auto oldThroughIn = throughIn;
        sink->disconnectSourcePipesShallow();
        this->disconnectSourcePipesShallow(); // disconnect throughIn
        oldSink->connectSourcePipe(oldThroughIn);
    }
    if (hasSource() && hasThroughOut()) {
        auto oldSource = source;
        auto oldThroughOut = throughOut;
        source->disconnectSinkPipesShallow();
        this->disconnectSinkPipesShallow(); // disconnect throughOut
        oldSource->connectSinkPipe(oldThroughOut);
    }
    if (hasSink())
        sink->disconnectSourcePipesShallow();

    if (hasSource())
        source->disconnectSinkPipesShallow();

    if (hasThroughIn() || hasThroughOut())
        FATAL_ERROR(F("Invalid state"), 0x9147); // LCOV_EXCL_LINE
}

MIDI_Pipe::~MIDI_Pipe() { disconnect(); }

#if defined(ESP32) || !defined(ARDUINO)
static std::mutex pipe_exclusive_mutex;
#endif

void MIDI_Pipe::exclusive(cn_t cn, bool exclusive) {
#if defined(ESP32) || !defined(ARDUINO)
    std::lock_guard<std::mutex> lock_guard(pipe_exclusive_mutex);
#endif
    if (hasSink())
        sink->lockDownstream(cn, exclusive);
    if (hasThroughIn())
        throughIn->lockUpstream(cn, exclusive);
}

END_CS_NAMESPACE

AH_DIAGNOSTIC_POP()