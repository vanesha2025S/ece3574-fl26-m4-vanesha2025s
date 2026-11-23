#pragma once

namespace aiws::gui {

class CorpusLifecycle {
    struct State {
        virtual ~State() = default;
        virtual bool can_build() const { return true; }
        virtual bool can_process() const { return false; }
        virtual bool request_build(CorpusLifecycle& life) const;
        virtual bool invalidate(CorpusLifecycle& life) const;
        virtual bool finish(CorpusLifecycle&, bool) const { return false; }
    };

    struct NotBuilt final : State {};
    struct Failed final : State {};
    struct Current final : State {
        bool can_process() const override { return true; }
    };

    struct Building final : State {
        bool can_build() const override { return false; }
        bool request_build(CorpusLifecycle&) const override { return false; }
        bool invalidate(CorpusLifecycle& life) const override {
            State::invalidate(life);
            return true;
        }
        bool finish(CorpusLifecycle& life, bool success) const override {
            if (success) {
                life.state_ = &life.current_;
            } else {
                life.state_ = &life.failed_;
            }
            return true;
        }
    };

    NotBuilt not_built_;
    Building building_;
    Current current_;
    Failed failed_;
    const State* state_ = &not_built_;

public:
    CorpusLifecycle() = default;
    CorpusLifecycle(const CorpusLifecycle&) = delete;
    CorpusLifecycle& operator=(const CorpusLifecycle&) = delete;

    bool can_build() const { return state_->can_build(); }
    bool can_process() const { return state_->can_process(); }
    bool request_build() { return state_->request_build(*this); }
    // Returns true when the workspace change interrupted an active build.
    bool invalidate() { return state_->invalidate(*this); }
    // Only Building accepts a completion.
    bool finish(bool success) { return state_->finish(*this, success); }
};

inline bool CorpusLifecycle::State::request_build(CorpusLifecycle& life) const {
    life.state_ = &life.building_;
    return true;
}

inline bool CorpusLifecycle::State::invalidate(CorpusLifecycle& life) const {
    life.state_ = &life.not_built_;
    return false;
}

}  // namespace aiws::gui
