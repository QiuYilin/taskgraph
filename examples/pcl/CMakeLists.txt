find_package(PCL CONFIG REQUIRED)

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

target_link_libraries(pcl_example PRIVATE ${PCL_LIBRARIES})
target_link_libraries(pcl_example PRIVATE taskgraph::Core taskgraph::Ui)