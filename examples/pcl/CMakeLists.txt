find_package(PCL REQUIRED COMPONENTS io)

add_executable(pcl_example main.cpp
                           PclNodeData.hpp
                           PclNode.hpp
                           PclNode.cpp
                           PointCloudReaderNode.hpp
                           PointCloudReaderNode.cpp
                           PointCloudWriterNode.hpp
                           PointCloudWriterNode.cpp
                           #imgui_demo.cpp
                           )

target_link_libraries(pcl_example PRIVATE ${PCL_LIBRARIES})#https://github.com/PointCloudLibrary/pcl/issues/4422
target_link_libraries(pcl_example PRIVATE taskgraph::Core taskgraph::Ui)
