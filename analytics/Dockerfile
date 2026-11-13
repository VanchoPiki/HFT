FROM eclipse-temurin:25-jdk AS build
WORKDIR /src
COPY . .
# Tests need Docker (Testcontainers) and run in CI/locally, not inside the image build.
RUN ./gradlew bootJar --no-daemon --console=plain

FROM eclipse-temurin:25-jre
WORKDIR /app
COPY --from=build /src/build/libs/analytics-0.0.1-SNAPSHOT.jar app.jar
EXPOSE 8080
ENTRYPOINT ["java", "-XX:MaxRAMPercentage=50", "-jar", "app.jar"]
