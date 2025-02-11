#pragma once

#include <memory>
#include <list>

enum class SemmetyGroupLayout {
	SplitH,
	SplitV,
	Tabbed,
};

class SemmetyParentFrame;

class SemmetyFrame {
public:
    virtual ~SemmetyFrame() = default;

    WP<SemmetyParentFrame> parent;

    virtual bool is_window() const { return false; }
    virtual bool is_empty() const { return false; }
    virtual bool is_leaf() const { return is_empty() || is_window(); }
    virtual void print() const = 0;

    SP<SemmetyParentFrame> get_parent() const;
  	Vector2D position;
  	Vector2D size;
  	Vector2D gap_topleft_offset;
  	Vector2D gap_bottomright_offset;
};

class SemmetyWindowFrame : public SemmetyFrame {
public:
    PHLWINDOWREF window;

    SemmetyWindowFrame(PHLWINDOWREF w);
    bool is_window() const override;
    bool is_empty() const override { return true; }
    bool is_empty() const override;
    void print() const override;
};

class SemmetyEmptyFrame : public SemmetyFrame {
public:
    void print() const override;
};

class SemmetyParentFrame : public SemmetyFrame {
public:
    std::list<SP<SemmetyFrame>> children;

    SemmetyParentFrame(std::list<SP<SemmetyFrame>> ch);
    void print() const override;
};
