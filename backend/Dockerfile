FROM node:20-alpine
RUN apk add --no-cache python3 make g++
WORKDIR /app
COPY package.json ./
RUN npm install --omit=dev
COPY src ./src
ENV PORT=3000
ENV DB_PATH=/data/aquaculture.db
EXPOSE 3000
CMD ["node", "src/index.js"]
