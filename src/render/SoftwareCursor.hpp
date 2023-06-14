#ifndef __SOFTWARECURSOR_HPP__
#define __SOFTWARECURSOR_HPP__

class SoftwareCursor
{
public:
    SoftwareCursor();
    ~SoftwareCursor();

    void SetPosition( float x, float y );
    void Render();

private:
    double m_x = 0;
    double m_y = 0;
};

#endif
