node('linux && docker') {
    def dockerImageName = "gfieni/hwpc-sensor:${env.BUILD_TAG}"

    stage('Checkout') {
        checkout scm
    }

    stage('Build') {
        sh "docker build -t ${dockerImageName} --build-arg BUILD_TYPE=release --target builder ."
    }
}
