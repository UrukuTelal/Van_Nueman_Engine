#ifndef COLLABORATION_WINDOW_H
#define COLLABORATION_WINDOW_H

#include <vector>
#include <string>

struct CollaborationMsg {
    std::string timestamp;
    std::string agent;
    std::string message;
    float delta_h;
};

void render_collaboration_window(std::vector<CollaborationMsg>& messages);

#endif // COLLABORATION_WINDOW_H
