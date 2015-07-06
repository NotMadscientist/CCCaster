#pragma once


class MouseManager
{
public:

    struct Owner
    {
        virtual void mouseEvent ( int x, int y, bool isDown, bool pressed, bool released ) = 0;
    };

    Owner *owner = 0;

private:

    // TODO implement hooks when needed

public:

    static MouseManager& get();
};
