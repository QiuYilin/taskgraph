find_package(OpenCV CONFIG REQUIRED)

add_executable(cv_example main.cpp
                          CvNodeData.hpp
                          CvNode.hpp
                          CvNode.cpp
                          ImageReaderNode.hpp
                          ImageReaderNode.cpp
                          MatchTemplateNode.hpp
                          MatchTemplateNode.cpp
                          #imgui_demo.cpp
              )

target_link_libraries(cv_example PRIVATE taskgraph::Core taskgraph::Ui opencv_core opencv_imgproc opencv_highgui)
