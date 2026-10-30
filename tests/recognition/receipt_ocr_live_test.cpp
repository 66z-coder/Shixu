// Opt-in real-engine smoke test: does not touch inventory or user data.
#include "recognition/ReceiptOcrService.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        if (argc != 2) throw std::runtime_error("usage: receipt_ocr_live_test cases.json");
        QFile cases(QString::fromLocal8Bit(argv[1]));
        if (!cases.open(QIODevice::ReadOnly)) throw std::runtime_error("cannot open cases");
        const auto inputs = QJsonDocument::fromJson(cases.readAll()).array();
        if (inputs.isEmpty()) throw std::runtime_error("empty case list");
        ReceiptOcrService service;
        QJsonArray timings;
        for (const auto& input : inputs) {
            const auto item = input.toObject();
            QFile expectedFile(item["expected"].toString());
            if (!expectedFile.open(QIODevice::ReadOnly)) throw std::runtime_error("cannot open expected result");
            const auto expected = ReceiptOcrService::parseResult(expectedFile.readAll());
            QEventLoop loop;
            ReceiptOcrResult actual;
            bool completed = false;
            const auto connection = QObject::connect(&service, &ReceiptOcrService::completed, &loop,
                [&](const ReceiptOcrResult& value) { actual = value; completed = true; loop.quit(); });
            QElapsedTimer timer;
            timer.start();
            service.start(item["image"].toString());
            if (!completed) loop.exec();
            QObject::disconnect(connection);
            if (!completed || actual.success != expected.success || actual.lines.size() != expected.lines.size())
                throw std::runtime_error(("unexpected result: " + actual.errorMessage).toStdString());
            for (int i = 0; i < actual.lines.size(); ++i) {
                if (actual.lines[i].text != expected.lines[i].text || actual.lines[i].box != expected.lines[i].box
                    || actual.lines[i].confidence != expected.lines[i].confidence)
                    throw std::runtime_error("OCR output differs from baseline");
            }
            const QJsonObject timing{{"image", item["image"]}, {"milliseconds", timer.elapsed()}, {"success", actual.success}};
            timings.append(timing);
            std::cout << QJsonDocument(timing).toJson(QJsonDocument::Compact).constData() << std::endl;
        }
        std::cout << "PASS: real Qt OCR service matches baseline and reuses worker\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
