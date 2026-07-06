{
    "Source": "HammerWireframe.azsl",
    "DepthStencilState": {
        "Depth": {
            "Enable": true,
            "WriteMask": "Zero",
            "CompareFunc": "Less"
        }
    },
    "RasterState": {
        "FillMode": "Wireframe",
        "CullMode": "None"
    },
    "GlobalTargetBlendState": {
        "Enable": true,
        "BlendSource": "AlphaSource",
        "BlendDest": "AlphaSourceInverse",
        "BlendOp": "Add"
    },
    "DrawList": "hammerwireframehidden",
    "ProgramSettings": {
        "EntryPoints": [
            {
                "name": "MainVS",
                "type": "Vertex"
            },
            {
                "name": "HiddenPS",
                "type": "Fragment"
            }
        ]
    }
}
