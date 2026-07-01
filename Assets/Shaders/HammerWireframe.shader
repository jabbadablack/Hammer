{
    "Source" : "HammerWireframe.azsl",

    "DrawList" : "hammerwireframe",

    "DepthStencilState" :
    {
        "Depth" : { "Enable" : true, "CompareFunc" : "GreaterEqual" }
    },

    "RasterState" :
    {
        "FillMode" : "Wireframe",
        "CullMode" : "None"
    },

    "ProgramSettings":
    {
      "EntryPoints":
      [
        {
          "name": "MainVS",
          "type": "Vertex"
        },
        {
          "name": "MainPS",
          "type": "Fragment"
        }
      ]
    }
}
