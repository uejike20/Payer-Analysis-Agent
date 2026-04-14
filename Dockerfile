FROM gcc:14 AS build

WORKDIR /src

COPY AnalysisPipeline ./AnalysisPipeline

RUN mkdir -p /out && g++ -std=c++17 -O2 \
    AnalysisPipeline/main.cpp \
    AnalysisPipeline/Payers.cpp \
    AnalysisPipeline/ProviderManual.cpp \
    AnalysisPipeline/PhoneTranscript.cpp \
    AnalysisPipeline/WebPage.cpp \
    AnalysisPipeline/DenialLetter.cpp \
    -o /out/authpipeline

FROM gcc:14 AS runtime

WORKDIR /app/AnalysisPipeline

RUN useradd -m -u 1000 appuser

COPY --from=build /out/authpipeline /usr/local/bin/authpipeline
COPY --from=build /src/AnalysisPipeline/payer_sources ./payer_sources

RUN chown -R appuser:appuser /app/AnalysisPipeline

USER 1000:1000

ENTRYPOINT ["/usr/local/bin/authpipeline"]
