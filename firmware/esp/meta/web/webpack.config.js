const path = require("path");

// Используемые плагины
const TerserPlugin = require("terser-webpack-plugin");
const HtmlWebpackPlugin = require("html-webpack-plugin");
const MiniCssExtractPlugin = require("mini-css-extract-plugin");
const CssMinimizerPlugin = require("css-minimizer-webpack-plugin");

// Режим сброки
const mode = process.env.NODE_ENV || "development";
// Признак режима разработки
const devMode = mode === "development";
// Конечная цель
const target = devMode ? "web" : "browserslist";
// Признак генерации карты исходников
const devtool = devMode ? "source-map" : undefined;

module.exports =
{
    mode,
    target,
    devtool,
    devServer: {
        port: 5000,
        open: true,
    },
    entry: path.resolve(__dirname, "src", "js", "main.js"),
    output: {
        path: path.resolve(__dirname, "dist"),
        clean: true,
        filename: "[name].js"
    },
    plugins: [
        new HtmlWebpackPlugin({
            template: path.resolve(__dirname, "src", "index.html"),
            inject: "body",
        }),
        new MiniCssExtractPlugin({
            filename: "[name].css"
        }),
    ],
    module: {
        rules: [
            {
                test: /\.html$/i,
                loader: "html-loader",
            },
            {
                test: /\.css$/i,
                use: [
                    devMode ? "style-loader" : MiniCssExtractPlugin.loader, 
                    "css-loader"
                ],
            },
            {
                test: /\.(svg|json)$/i,
                type: "asset/resource",
                generator: {
                    // Удержание оригинального имени
                    filename: '[name][ext]', 
                },
            },
        ]
    },
    optimization: {
        minimizer: [
            new CssMinimizerPlugin(),
            new TerserPlugin({ extractComments: false, }),
        ],
    },
    performance: {
        hints: false,
        maxAssetSize: 512000,
        maxEntrypointSize: 512000,
    },
}